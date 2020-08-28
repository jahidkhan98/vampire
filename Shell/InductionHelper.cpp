#include "InductionHelper.hpp"

#include "Kernel/Formula.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/SubstHelper.hpp"
#include "Kernel/Substitution.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/TermIterators.hpp"
#include "Kernel/Unit.hpp"

using namespace Kernel;

namespace Shell {

TermList TermListReplacement::transformSubterm(TermList trm)
{
  CALL("TermListReplacement::transformSubterm");

  if(trm.isVar() && _o.isVar() && trm.var() == _o.var()) {
    return _r;
  }

  if(trm.isTerm() && _o.isTerm() && trm.term()==_o.term()){
    return _r;
  }
  return trm;
}

TermList TermOccurrenceReplacement::transformSubterm(Kernel::TermList trm)
{
  CALL("TermOccurrenceReplacement::transformSubterm");

  if (trm.isVar() || _r.count(trm) == 0) {
    return trm;
  }

  if (!_c.find(trm)) {
    _c.insert(trm, 0);
  } else {
    _c.get(trm)++;
  }

  // The induction generalization heuristic is stored here:
  // - if we have only one active occurrence, induct on all
  // - otherwise only induct on the active occurrences
  const auto& o = _o.get(trm);
  if (o->size() == 1 || o->contains(_c.get(trm))) {
    return _r.at(trm);
  }
  return trm;
}

TermList VarShiftReplacement::transformSubterm(TermList trm)
{
  CALL("VarShiftReplacement::transformSubterm");

  if(trm.isVar()) {
    return TermList(trm.var()+_shift, trm.isSpecialVar());
  }
  return trm;
}

TermList VarReplacement::transformSubterm(TermList trm)
{
  CALL("VarReplacement::transformSubterm");

  if(trm.isVar()) {
    if (!_varMap.find(trm.var())) {
      _varMap.insert(trm.var(), _v++);
    }
    return TermList(_varMap.get(trm.var()), false);
  }
  return trm;
}

bool IteratorByInductiveVariables::hasNext()
{
  ASS(_it.hasNext() == (_indVarIt != _end));

  while (_indVarIt != _end && !*_indVarIt) {
    _indVarIt++;
    _it.next();
  }
  return _indVarIt != _end;
}

TermList IteratorByInductiveVariables::next()
{
  ASS(hasNext());
  _indVarIt++;
  return _it.next();
}

RDescription::RDescription(const vvector<TermList>& recursiveCalls, TermList step, Formula* cond)
  : _recursiveCalls(recursiveCalls),
    _step(step),
    _condition(cond)
{}

RDescription::RDescription(TermList step, Formula* cond)
  : _recursiveCalls(),
    _step(step),
    _condition(cond)
{}

ostream& operator<<(ostream& out, const RDescription& rdesc)
{
  bool empty = rdesc._recursiveCalls.empty();
  if (!empty) {
    out << "(";
  }
  unsigned n = 0;
  for (const auto& r : rdesc._recursiveCalls) {
    out << r;
    if (++n < rdesc._recursiveCalls.size()) {
      out << " & ";
    }
  }
  if (!empty) {
    out << ") => ";
  }
  out << rdesc._step;
  return out;
}

RDescriptionInst::RDescriptionInst(vvector<vmap<TermList, TermList>>&& recursiveCalls,
                                   vmap<TermList, TermList>&& step, Formula* cond)
  : _recursiveCalls(recursiveCalls),
    _step(step),
    _condition(cond)
{}

ostream& operator<<(ostream& out, const RDescriptionInst& inst)
{
  out << "recursive calls: ";
  for (const auto& r : inst._recursiveCalls) {
    for (const auto& kv : r) {
      out << kv.first << " -> " << kv.second << "; ";
    }
  }
  out << "step: ";
  for (const auto& kv : inst._step) {
    out << kv.first << " -> " << kv.second << "; ";
  }
  return out;
}

ostream& operator<<(ostream& out, const InductionTemplate& templ)
{
  out << "RDescriptions:";
  unsigned n = 0;
  for (const auto& rdesc : templ._rDescriptions) {
    out << rdesc;
    if (++n < templ._rDescriptions.size()) {
      out << "; ";
    }
  }
  n = 0;
  out << " with inductive positions: (";
  for (const auto& b : templ._inductionVariables) {
    out << Int::toString(b);
    if (++n < templ._inductionVariables.size()) {
      out << ",";
    }
  }
  out << ")";
  return out;
}

void InductionTemplate::postprocess()
{
  ASS(!_rDescriptions.empty());
  _rDescriptions.shrink_to_fit();

  _inductionVariables = vvector<bool>(_rDescriptions[0]._step.term()->arity(), false);
  for (auto& rdesc : _rDescriptions) {
    auto step = rdesc._step.term();
    for (auto& r : rdesc._recursiveCalls) {
      Term::Iterator argIt1(r.term());
      Term::Iterator argIt2(step);
      unsigned i = 0;
      while (argIt1.hasNext()) {
        ASS(argIt2.hasNext());
        auto t1 = argIt1.next();
        auto t2 = argIt2.next();
        if (t1 != t2 && t2.containsSubterm(t1)) {
          _inductionVariables[i] = true;
          // cout << t2.toString() << " properly contains " << t1.toString() << endl;
        } else {
          // cout << t2.toString() << " does not properly contain " << t1.toString() << endl;
        }
        i++;
      }
    }
  }
}

void InductionScheme::init(Term* t, vvector<RDescription>& rdescs, const vvector<bool>& indVars)
{
  CALL("InductionScheme::init");

  unsigned var = 0;
  for (auto& rdesc : rdescs) {
    DHMap<unsigned, unsigned> varMap;
    vmap<TermList,TermList> stepSubst;

    IteratorByInductiveVariables termIt(t, indVars);
    IteratorByInductiveVariables stepIt(rdesc._step.term(), indVars);

    bool mismatch = false;
    while (termIt.hasNext()) {
      auto argTerm = termIt.next();
      auto argStep = stepIt.next();
      auto its = InductionHelper::getInductionTerms(argTerm);
      for (auto& indTerm : its) {
        if (stepSubst.count(indTerm) > 0) {
          if (stepSubst.at(indTerm).isTerm() && argStep.isTerm() &&
              stepSubst.at(indTerm).term()->functor() != argStep.term()->functor()) {
            mismatch = true;
            break;
          }
          continue;
        }
        // there may be induction variables which
        // don't change in some cases
        if (argStep.isVar()) {
          continue;
        }
        VarReplacement cr(varMap, var);
        auto res = cr.transform(argStep.term());
        stepSubst.insert(make_pair(indTerm, TermList(res)));
      }
    }
    if (mismatch) {
      // We cannot properly create this case because
      // there is a mismatch between the ctors for
      // a substituted term
      continue;
    }

    vvector<vmap<TermList,TermList>> recCallSubstList;
    for (auto& r : rdesc._recursiveCalls) {
      vmap<TermList,TermList> recCallSubst;

      IteratorByInductiveVariables termIt(t, indVars);
      IteratorByInductiveVariables recCallIt(r.term(), indVars);

      while (termIt.hasNext()) {
        auto argTerm = termIt.next();
        auto argRecCall = recCallIt.next();
        auto its = InductionHelper::getInductionTerms(argTerm);
        for (auto& indTerm : its) {
          if (recCallSubst.count(indTerm) > 0) {
            continue;
          }
          if (argRecCall.isVar()) {
            // first we check if this variable corresponds to at least one complex term 
            // in the step (it is an induction variable position but may not be
            // changed in this case)
            IteratorByInductiveVariables stepIt(rdesc._step.term(), indVars);
            bool found = false;
            while (stepIt.hasNext()) {
              auto argStep = stepIt.next();
              if (argStep != argRecCall && argStep.containsSubterm(argRecCall)) {
                found = true;
                break;
              }
            }
            if (found) {
              recCallSubst.insert(make_pair(
                indTerm, TermList(varMap.get(argRecCall.var()), false)));
            }
          } else {
            VarReplacement cr(varMap, var);
            auto res = cr.transform(argRecCall.term());
            recCallSubst.insert(make_pair(indTerm, TermList(res)));
          }
        }
      }
      recCallSubstList.push_back(std::move(recCallSubst));
    }
    _rDescriptionInstances.emplace_back(std::move(recCallSubstList), std::move(stepSubst), nullptr);
  }
  _rDescriptionInstances.shrink_to_fit();
  _maxVar = var;
}

ostream& operator<<(ostream& out, const InductionScheme& scheme)
{
  out << "RDescription instances: ";
  for (const auto& inst : scheme._rDescriptionInstances) {
    out << inst << " ;-- ";
  }
  return out;
}

void InductionHelper::preprocess(Problem& prb)
{
  preprocess(prb.units());
}

void InductionHelper::preprocess(UnitList* units)
{
  UnitList::Iterator it(units);
  while (it.hasNext()) {
    auto unit = it.next();
    if (unit->isClause()){
      continue;
    }

    auto formula = unit->getFormula();
    while (formula->connective() == Connective::FORALL) {
      formula = formula->qarg();
    }

    if (formula->connective() != Connective::LITERAL) {
      continue;
    }

    auto lit = formula->literal();

    if (!lit->isRecFuncDef()) {
      continue;
    }
    auto lhs = lit->nthArgument(0);
    auto rhs = lit->nthArgument(1);
    auto lhterm = lhs->term();
    bool isPred = lhterm->isFormula();
    if (isPred) {
      lhterm = lhterm->getSpecialData()->getFormula()->literal();
    }

    InductionTemplate templ;
    TermList term(lhterm);
    processBody(*rhs, term, templ);
    templ.postprocess();

    if(env.options->showInduction()){
      env.beginOutput();
      env.out() << "[Induction] recursive function: " << lit << ", with induction template: " << templ << endl;
      env.endOutput();
    }
    env.signature->addInductionTemplate(lhterm->functor(), isPred, std::move(templ));
  }
}

void InductionHelper::filterSchemes(vvector<InductionScheme>& schemes)
{
  CALL("InductionHelper::filterSchemes");

  for (unsigned i = 0; i < schemes.size(); i++) {
    for (unsigned j = i+1; j < schemes.size();) {
      if (checkSubsumption(schemes[j], schemes[i])) {
        if(env.options->showInduction()){
          env.beginOutput();
          env.out() << "[Induction] induction scheme " << schemes[j] << " is subsumed by " << schemes[i] << endl;
          env.endOutput();
        }
        schemes[j] = std::move(schemes.back());
        schemes.pop_back();
      } else {
        j++;
      }
    }
  }
}

bool InductionHelper::canInductOn(TermList t)
{
  if (t.isVar()) {
    return false;
  }
  auto fn = t.term()->functor();
  auto symb = t.term()->isLiteral() ? env.signature->getPredicate(fn) : env.signature->getFunction(fn);
  return symb->skolem();
}

bool InductionHelper::isTermAlgebraCons(TermList t) {
  if (t.isVar()) { return false; }
  auto func = t.term()->functor();
  auto symb = t.term()->isLiteral() ? env.signature->getPredicate(func) : env.signature->getFunction(func);
  return symb->termAlgebraCons();
}

OperatorType* getType(TermList t) {
  auto fn = t.term()->functor();
  auto symb = t.term()->isLiteral() ? env.signature->getPredicate(fn) : env.signature->getFunction(fn);
  return t.term()->isLiteral() ? symb->predType() : symb->fnType();
}

vvector<TermList> InductionHelper::getInductionTerms(TermList t)
{
  vvector<TermList> v;
  if (t.isVar()) {
    return v;
  }
  if (canInductOn(t)) {
    v.push_back(t);
    return v;
  }
  if (!isTermAlgebraCons(t)) {
    return v;
  }
  auto type = getType(t);
  //TODO(mhajdu): eventually check whether we really recurse on a specific
  // subterm of the constructor terms
  Stack<pair<TermList, bool>> actStack;
  actStack.push(make_pair(t, true));
  while (actStack.isNonEmpty()) {
    auto kv = actStack.pop();
    auto st = kv.first;
    auto active = kv.second;
    if (st.isVar()) {
      continue;
    }
    if (active && canInductOn(st) && getType(st)->result() == type->result()) {
      v.push_back(st);
    }
    if (active && isTermAlgebraCons(st)) {
      for (unsigned i = 0; i < st.term()->arity(); i++) {
        actStack.push(make_pair(*st.term()->nthArgument(i),true));
      }
    }
  }
  return v;
}

void InductionHelper::processBody(TermList& body, TermList& header, InductionTemplate& templ)
{
  if (body.isVar()) {
    templ._rDescriptions.emplace_back(header, nullptr);
    return;
  }
  auto term = body.term();
  if (!term->isSpecial() || term->isFormula()) {
    vvector<TermList> recursiveCalls;
    processCase(header.term()->functor(), body, recursiveCalls);
    templ._rDescriptions.emplace_back(recursiveCalls, header, nullptr);
  }
  else if (term->isMatch())
  {
    auto matchedVar = term->nthArgument(0)->var();
    unsigned index = findMatchedArgument(matchedVar, header);
    ASS(index < header.term()->arity());

    for (unsigned i = 1; i < term->arity(); i+=2) {
      auto pattern = term->nthArgument(i);
      auto matchBody = term->nthArgument(i+1);
      TermListReplacement tr(TermList(matchedVar,false), *pattern);
      TermList t(tr.transform(header.term()));
      processBody(*matchBody, t, templ);
    }
  }
}

void InductionHelper::processCase(const unsigned recFun, TermList& body, vvector<TermList>& recursiveCalls)
{
  if (!body.isTerm()) {
    return;
  }

  auto term = body.term();
  if (term->functor() == recFun) {
    recursiveCalls.push_back(body);
  }

  if (term->isFormula()) {
    auto formula = term->getSpecialData()->getFormula();
    switch (formula->connective()) {
      case LITERAL: {
        TermList lit(formula->literal());
        processCase(recFun, lit, recursiveCalls);
        break;
      }
      case AND:
      case OR: {
        FormulaList::Iterator it(formula->args());
        while (it.hasNext()) {
          // TODO(mhajdu): maybe don't create a new Term here
          TermList ft(Term::createFormula(it.next()));
          processCase(recFun, ft, recursiveCalls);
        }
        break;
      }
      case TRUE:
      case FALSE: {
        break;
      }
#if VDEBUG
      default:
        ASSERTION_VIOLATION;
#endif
    }
  } else {
    Term::Iterator it(term);
    while (it.hasNext()) {
      auto n = it.next();
      processCase(recFun, n, recursiveCalls);
    }
  }
}

unsigned InductionHelper::findMatchedArgument(unsigned matched, TermList& header)
{
  unsigned i = 0;
  Term::Iterator argIt(header.term());
  while (argIt.hasNext()) {
    IntList::Iterator varIt(argIt.next().freeVariables());
    bool found = false;
    while (varIt.hasNext()) {
      auto var = varIt.next();
      if (var == matched) {
        found = true;
        break;
      }
    }
    if (found) {
      break;
    }
    i++;
  }
  return i;
}

vstring substTermsToString(List<Term*>* l) {
  vstring str;
  List<Term*>::Iterator it(l);
  while (it.hasNext()) {
    str+=it.next()->toString()+"; ";
  }
  return str;
}

bool equalsUpToVariableRenaming(TermList t1, TermList t2) {
  if (t1.isVar() && t2.isVar()) {
    return true;
  }
  if (t1.isVar()) {
    return false;
  }
  if (t2.isVar()) {
    return false;
  }

  auto tt1 = t1.term();
  auto tt2 = t2.term();
  if (tt1->functor() != tt2->functor() || tt1->arity() != tt2->arity())
  {
    return false;
  }

  Term::Iterator it1(tt1);
  Term::Iterator it2(tt2);
  while (it1.hasNext()) {
    if (!equalsUpToVariableRenaming(it1.next(), it2.next())) {
      return false;
    }
  }
  return true;
}

bool containsUpToVariableRenaming(TermList container, TermList contained) {
  if (contained.isVar()) {
    return true;
  }
  if (container.isVar()) {
    return false;
  }

  auto t1 = container.term();
  auto t2 = contained.term();
  if (t1->functor() == t2->functor() && t1->arity() == t2->arity())
  {
    bool equal = true;
    Term::Iterator it1(t1);
    Term::Iterator it2(t2);
    while (it1.hasNext()) {
      auto arg1 = it1.next();
      auto arg2 = it2.next();
      if (!equalsUpToVariableRenaming(arg1, arg2)) {
        equal = false;
        break;
      }
    }
    if (equal) {
      return true;
    }
  }

  Term::Iterator it1(container.term());
  while (it1.hasNext()) {
    auto arg1 = it1.next();
    if (containsUpToVariableRenaming(arg1, contained)) {
      return true;
    }
  }
  return false;
}

bool InductionHelper::checkSubsumption(const InductionScheme& sch1, const InductionScheme& sch2, bool onlyCheckIntersection)
{
  for (const auto& rdesc1 : sch1._rDescriptionInstances) {
    auto contained = false;
    for (const auto& rdesc2 : sch2._rDescriptionInstances) {
      if (rdesc2._recursiveCalls.empty() != rdesc1._recursiveCalls.empty()) {
        continue;
      }
      bool contained1 = true;
      for (const auto& kv : rdesc1._step) {
        if (rdesc2._step.count(kv.first) == 0) {
          if (!onlyCheckIntersection) {
            contained1 = false;
          }
          break;
        }
        const auto& s2 = rdesc2._step.at(kv.first);
        if (!containsUpToVariableRenaming(s2, kv.second)) {
          contained1 = false;
          break;
        }
      }
      if (contained1) {
        contained = true;
        break;
      }
    }
    if (!contained) {
      return false;
    }
  }
  return true;
}

TermList shiftVarsUp(TermList t, unsigned shift) {
  if (t.isVar()) {
    return TermList(t.var()+shift, t.isSpecialVar());
  }
  VarShiftReplacement vr(shift);
  return TermList(vr.transform(t.term()));
}

} // Shell
