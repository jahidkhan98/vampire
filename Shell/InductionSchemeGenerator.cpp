#include "InductionSchemeGenerator.hpp"

#include "Kernel/Formula.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/TermIterators.hpp"
#include "Kernel/Unit.hpp"

using namespace Kernel;

namespace Shell {

bool isSkolem(TermList t) {
  CALL("isSkolem");

  if (t.isVar()) {
    return false;
  }
  auto fn = t.term()->functor();
  auto symb = t.term()->isLiteral() ? env.signature->getPredicate(fn) : env.signature->getFunction(fn);
  return symb->skolem();
}

bool canInductOn(TermList t)
{
  CALL("canInductOn");

  if (t.isVar()) {
    return false;
  }
  return isSkolem(t);

  // induct on complex terms
  // return true;
}

bool isTermAlgebraCons(TermList t)
{
  CALL("isTermAlgebraCons");

  if (t.isVar()) {
    return false;
  }
  auto func = t.term()->functor();
  auto symb = t.term()->isLiteral() ? env.signature->getPredicate(func) : env.signature->getFunction(func);
  return symb->termAlgebraCons();
}

OperatorType* getType(TermList t)
{
  CALL("getType");

  //TODO(mhajdu): maybe handle variables?
  auto fn = t.term()->functor();
  auto symb = t.term()->isLiteral() ? env.signature->getPredicate(fn) : env.signature->getFunction(fn);
  return t.term()->isLiteral() ? symb->predType() : symb->fnType();
}

/**
 * Returns all subterms which can be inducted on for a term.
 */
vvector<TermList> getInductionTerms(TermList t)
{
  CALL("getInductionTerms");

  vvector<TermList> v;
  if (t.isVar()) {
    return v;
  }
  if (canInductOn(t)) {
    v.push_back(t);
    return v;
  }
  unsigned f = t.term()->functor();
  bool isPred = t.term()->isLiteral();

  // If function with recursive definition,
  // recurse in its active arguments
  if (env.signature->hasInductionTemplate(f, isPred)) {
    auto& templ = env.signature->getInductionTemplate(f, isPred);
    const auto& indVars = templ._inductionVariables;

    IteratorByInductiveVariables argIt(t.term(), indVars);
    while (argIt.hasNext()) {
      auto arg = argIt.next();
      auto indTerms = getInductionTerms(arg);
      v.insert(v.end(), indTerms.begin(), indTerms.end());
    }
  }
  if (isTermAlgebraCons(t)) {
    auto type = getType(t);
    //TODO(mhajdu): eventually check whether we really recurse on a specific
    // subterm of the constructor terms
    for (unsigned i = 0; i < t.term()->arity(); i++) {
      auto st = *t.term()->nthArgument(i);
      if (st.isVar()) {
        continue;
      }
      if (getType(st)->result() == type->result()) {
        auto indTerms = getInductionTerms(st);
        v.insert(v.end(), indTerms.begin(), indTerms.end());
      }
    }
  }
  return v;
}

// bool matchesCase(TermList c, TermList t) {
//   CALL("matchesCase");

//   if (c.isVar()) {
//     return true;
//   }
//   if (canInductOn(t)) {
//     return false;
//   }

//   auto t1 = c.term();
//   auto t2 = t.term();
//   if (t1->isBoolean() != t2->isBoolean()
//     || t1->functor() != t2->functor()
//     || t1->arity() != t2->arity())
//   {
//     return false;
//   }
//   Term::Iterator it1(t1);
//   Term::Iterator it2(t2);
//   while (it1.hasNext()) {
//     auto arg1 = it1.next();
//     auto arg2 = it2.next();
//     if (!matchesCase(arg1, arg2)) {
//       return false;
//     }
//   }
//   return true;
// }

TermList TermOccurrenceReplacement::transformSubterm(TermList trm)
{
  CALL("TermOccurrenceReplacement::transformSubterm");

  auto rIt = _r.find(trm);

  // The induction generalization heuristic is stored here:
  // - if we have only one active occurrence, induct on all
  // - otherwise only induct on the active occurrences
  if (rIt != _r.end()) {
    if (!_c.find(trm)) {
      _c.insert(trm, 0);
    } else {
      _c.get(trm)++;
    }
    const auto& o = _o.get(trm);
    // auto oc = _oc.get(trm);
    if (o->size() == 1 /*|| oc == o->size() + 1*/ || o->contains(_c.get(trm))) {
      return _r.at(trm);
    }
  }
  // otherwise we may replace it with a variable
  if ((_replaceSkolem && isSkolem(trm)) || trm.isVar()) {
    if (!_r_g.count(trm)) {
      _r_g.insert(make_pair(trm, TermList(_v++,false)));
    }
    return _r_g.at(trm);
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

TermList VarShiftReplacement::transformSubterm(TermList trm) {
  if (trm.isVar()) {
    return TermList(trm.var()+_shift, trm.isSpecialVar());
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

ostream& operator<<(ostream& out, const RDescriptionInst& inst)
{
  if (!inst._conditions.empty()) {
    out << "* conditions: ";
    for (const auto& c : inst._conditions) {
      out << *c << ", ";
    }
    out << endl;
  }
  auto basecase = inst._recursiveCalls.empty();
  if (!basecase) {
    out << "** recursive calls: ";
    for (const auto& r : inst._recursiveCalls) {
      for (const auto& kv : r) {
        out << kv.first << " -> " << kv.second << ", ";
      }
      out << "; ";
    }
    out << endl;
  }
  if (basecase) {
    out << "** base: ";
  } else {
    out << "** step: ";
  }
  for (const auto& kv : inst._step) {
    out << kv.first << " -> " << kv.second << ", ";
  }
  out << endl;
  if (!inst._inactive.empty()) {
    out << "** inactive terms: ";
    for (const auto& i : inst._inactive) {
      out << i << ", ";
    }
    out << endl;
  }
  return out;
}

void InductionScheme::init(Term* t, vvector<RDescription>& rdescs, const vvector<bool>& indVars)
{
  CALL("InductionScheme::init");

  unsigned var = 0;
  for (auto& rdesc : rdescs) {
    // for each RDescription, use a new substitution and variable
    // replacement as these cases should be independent
    DHMap<unsigned, unsigned> varMap;
    vmap<TermList,TermList> stepSubst;

    IteratorByInductiveVariables termIt(t, indVars);
    IteratorByInductiveVariables stepIt(rdesc._step.term(), indVars);

    // We first map the inductive terms of t to the arguments of
    // the function header stored in the step case
    bool mismatch = false;
    bool match = termIt.hasNext();
    while (termIt.hasNext()) {
      auto argTerm = termIt.next();
      auto argStep = stepIt.next();
      auto its = getInductionTerms(argTerm);
      // if (!matchesCase(argStep, argTerm)) {
      //   match = false;
      // }
      for (auto& indTerm : its) {
        // This argument might have already been mapped
        if (stepSubst.count(indTerm) > 0) {
          if (stepSubst.at(indTerm).isTerm() && argStep.isTerm() &&
              stepSubst.at(indTerm).term()->functor() != argStep.term()->functor()) {
            // If this argument in the RDescription header contains a different
            // term algebra ctor than the already substituted one, we cannot create
            // this case
            mismatch = true;
            break;
          }
          continue;
        }
        // There may be variables in active
        // positions, these are skipped
        if (argStep.isVar()) {
          continue;
        }
        VarReplacement cr(varMap, var);
        auto res = cr.transform(argStep.term());
        stepSubst.insert(make_pair(indTerm, TermList(res)));
        _inductionTerms.insert(indTerm);
      }
    }
    // if (match) {
    //   // The literal can be simplified, so we don't induct on it yet
    //   return false;
    // }
    if (mismatch) {
      // We cannot properly create this case because
      // there is a mismatch between the ctors for
      // a substituted term
      continue;
    }

    // At this point all induction terms of t are mapped
    // and the case is valid, so we do the same with the
    // conditions and recursive calls too
    vvector<Formula*> condSubstList;
    for (auto& c : rdesc._conditions) {
      VarReplacement cr(varMap, var);
      auto res = cr.transform(c);
      condSubstList.push_back(res);
    }
    vvector<vmap<TermList,TermList>> recCallSubstList;
    for (auto& r : rdesc._recursiveCalls) {
      vmap<TermList,TermList> recCallSubst;

      IteratorByInductiveVariables termIt(t, indVars);
      IteratorByInductiveVariables recCallIt(r.term(), indVars);

      while (termIt.hasNext()) {
        auto argTerm = termIt.next();
        auto argRecCall = recCallIt.next();
        auto its = getInductionTerms(argTerm);
        for (auto& indTerm : its) {
          if (recCallSubst.count(indTerm) > 0) {
            continue;
          }
          if (argRecCall.isVar()) {
            // First we check if this variable is a subterm of some complex term
            // in the step (it is an induction variable position but may not be
            // changed in this case, see the check above)
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
    _rDescriptionInstances.emplace_back(std::move(recCallSubstList), std::move(stepSubst), std::move(condSubstList));
  }
  // We now collect inactive/passive terms by just
  // collecting any non-induction term. For the strongest
  // hypotheses however, we need to check on a case to case
  // basis and collect anything that is not changing
  Term::Iterator it(t);
  while(it.hasNext()) {
    auto arg = it.next();
    auto ind = getInductionTerms(arg);
    // ASS(ind.size() <= 1);
    if (!ind.empty() && !_inductionTerms.count(ind[0])) {
      _inactive.insert(ind[0]);
    }
  }
  for (auto& rdesc : _rDescriptionInstances) {
    rdesc._inactive = _inactive;
  }
  _rDescriptionInstances.shrink_to_fit();
  _maxVar = var;
  // return true;
}

void InductionScheme::init(vvector<RDescriptionInst>&& rdescs)
{
  CALL("InductionScheme::init");

  _rDescriptionInstances = rdescs;
  _inductionTerms.clear();
  unsigned var = 0;

  for (auto& rdesc : _rDescriptionInstances) {
    DHMap<unsigned, unsigned> varMap;
    VarReplacement vr(varMap, var);
    for (auto& kv : rdesc._step) {
      kv.second = kv.second.isVar()
        ? vr.transformSubterm(kv.second)
        : TermList(vr.transform(kv.second.term()));
      _inductionTerms.insert(kv.first);
    }
    for (auto& recCall : rdesc._recursiveCalls) {
      for (auto& kv : recCall) {
        kv.second = kv.second.isVar()
          ? vr.transformSubterm(kv.second)
          : TermList(vr.transform(kv.second.term()));
      }
    }
    _inactive.insert(rdesc._inactive.begin(), rdesc._inactive.end());
    for (auto& f : rdesc._conditions) {
      f = vr.transform(f);
    }
  }
  _maxVar = var;
}

InductionScheme InductionScheme::makeCopyWithVariablesShifted(unsigned shift) const {
  InductionScheme res;
  res._inductionTerms = _inductionTerms;
  res._inactive = _inactive;
  VarShiftReplacement vsr(shift);

  for (const auto& rdesc : _rDescriptionInstances) {
    vvector<vmap<TermList, TermList>> resRecCalls;
    for (const auto& recCall : rdesc._recursiveCalls) {
      vmap<TermList, TermList> resRecCall;
      for (auto kv : recCall) {
        resRecCall.insert(make_pair(kv.first,
          kv.second.isVar()
            ? vsr.transformSubterm(kv.second)
            : TermList(vsr.transform(kv.second.term()))));
      }
      resRecCalls.push_back(resRecCall);
    }
    vmap<TermList, TermList> resStep;
    for (auto kv : rdesc._step) {
      resStep.insert(make_pair(kv.first,
        kv.second.isVar()
          ? vsr.transformSubterm(kv.second)
          : TermList(vsr.transform(kv.second.term()))));
    }
    vvector<Formula*> resCond;
    for (auto f : rdesc._conditions) {
      resCond.push_back(vsr.transform(f));
    }
    res._rDescriptionInstances.emplace_back(std::move(resRecCalls),
      std::move(resStep), std::move(resCond));
  }
  for (auto& rdesc : res._rDescriptionInstances) {
    rdesc._inactive = res._inactive;
  }
  res._maxVar = _maxVar + shift;
  return res;
}

void InductionScheme::addInductionTerms(const vset<TermList>& terms) {
  for (const auto& t : terms) {
    for (auto& rdesc : _rDescriptionInstances) {
      if (rdesc._recursiveCalls.empty()) {
        continue;
      }
      auto it = rdesc._step.find(t);
      if (it == rdesc._step.end()) {
        TermList var(_maxVar++, false);
        rdesc._step.insert(make_pair(t, var));
        for (auto& recCall : rdesc._recursiveCalls) {
          recCall.insert(make_pair(t, var));
        }
      }
    }
  }
}

ostream& operator<<(ostream& out, const InductionScheme& scheme)
{
  out << endl;
  out << "* r-description instances: " << endl;
  auto i = 0;
  for (const auto& inst : scheme._rDescriptionInstances) {
    out << ++i << "." << endl << inst;
  }
  out << "induction terms: ";
  for (const auto& t : scheme._inductionTerms) {
    out << t << ", ";
  }
  if (!scheme._inactive.empty()) {
    out << " inactive terms: ";
    for (const auto& t : scheme._inactive) {
      out << t << ", ";
    }
  }

  return out;
}

InductionSchemeGenerator::~InductionSchemeGenerator()
{
  DHMap<Literal*, DHMap<TermList, DHSet<unsigned>*>*>::Iterator it(_actOccMaps);
  while (it.hasNext()) {
    DHMap<TermList, DHSet<unsigned>*>::Iterator aoIt(*it.next());
    while (aoIt.hasNext()) {
      delete aoIt.next();
    }
  }
  for (auto& kv : _primarySchemes) {
    delete kv.second;
  }
}

void InductionSchemeGenerator::generatePrimary(Clause* premise, Literal* lit)
{
  generate(premise, lit, _primarySchemes);
}

void InductionSchemeGenerator::generateSecondary(Clause* premise, Literal* lit)
{
  generate(premise, lit, _secondarySchemes);
}

void InductionSchemeGenerator::generate(Clause* premise, Literal* lit,
  vvector<pair<InductionScheme, DHMap<Literal*, Clause*>*>>& schemes)
{
  CALL("InductionSchemeGenerator::generate");

  // Process all subterms of the literal to
  // be able to store occurrences of induction
  // terms. The literal itself and both sides
  // of the equality count as active positions.
  if (_actOccMaps.find(lit)) {
    return;
  }
  _actOccMaps.insert(lit, new DHMap<TermList, DHSet<unsigned>*>());
  _currOccMaps.insert(lit, new DHMap<TermList, unsigned>());

  Stack<bool> actStack;
  if (lit->isEquality()) {
    actStack.push(true);
    actStack.push(true);
  } else {
    process(TermList(lit), true, actStack, premise, lit, schemes);
  }
  SubtermIterator it(lit);
  while(it.hasNext()){
    TermList curr = it.next();
    bool active = actStack.pop();
    process(curr, active, actStack, premise, lit, schemes);
  }
  ASS(actStack.isEmpty());
}

void InductionSchemeGenerator::process(TermList curr, bool active,
  Stack<bool>& actStack, Clause* premise, Literal* lit,
  vvector<pair<InductionScheme, DHMap<Literal*, Clause*>*>>& schemes)
{
  CALL("InductionSchemeGenerator::process");

  if (!curr.isTerm()) {
    return;
  }
  auto t = curr.term();

  // If induction term, store the occurrence
  if (canInductOn(curr)) {
    if (!_currOccMaps.get(lit)->find(curr)) {
      _currOccMaps.get(lit)->insert(curr, 0);
      _actOccMaps.get(lit)->insert(curr, new DHSet<unsigned>());
    }
    if (active) {
      _actOccMaps.get(lit)->get(curr)->insert(_currOccMaps.get(lit)->get(curr));
    }
    _currOccMaps.get(lit)->get(curr)++;
  }

  unsigned f = t->functor();
  bool isPred = t->isLiteral();

  // If function with recursive definition, create a scheme
  if (env.signature->hasInductionTemplate(f, isPred)) {
    auto& templ = env.signature->getInductionTemplate(f, isPred);
    const auto& indVars = templ._inductionVariables;

    for (auto it = indVars.rbegin(); it != indVars.rend(); it++) {
      actStack.push(*it && active);
    }

    IteratorByInductiveVariables argIt(t, indVars);
    bool match = true;
    while (argIt.hasNext()) {
      auto arg = argIt.next();
      auto its = getInductionTerms(arg);
      if (its.size() != 1) {
        match = false;
        break;
      }
    }

    if (match) {
      InductionScheme scheme;
      scheme.init(t, templ._rDescriptions, templ._inductionVariables);
      auto litClMap = new DHMap<Literal*, Clause*>();
      litClMap->insert(lit, premise);
      if(env.options->showInduction()){
        env.beginOutput();
        env.out() << "[Induction] induction scheme " << scheme
                  << " was suggested by term " << *t << endl;
        env.endOutput();
      }
      schemes.push_back(make_pair(std::move(scheme), litClMap));
    }
  // We induct on subterms of term algebra constructors
  } else if (isTermAlgebraCons(curr)) {
    for (unsigned i = 0; i < t->arity(); i++) {
      actStack.push(active);
    }
  } else {
    for (unsigned i = 0; i < t->arity(); i++) {
      actStack.push(false);
    }
  }
}

} // Shell