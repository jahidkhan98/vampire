/**
 * @file URResolution.cpp
 * Implements class URResolution.
 */

#include "Lib/DArray.hpp"
#include "Lib/Environment.hpp"
#include "Lib/Int.hpp"
#include "Lib/Metaiterators.hpp"
#include "Lib/PairUtils.hpp"
#include "Lib/VirtualIterator.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/ColorHelper.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/Inference.hpp"

#include "Indexing/Index.hpp"
#include "Indexing/LiteralIndex.hpp"
#include "Indexing/IndexManager.hpp"

#include "Saturation/SaturationAlgorithm.hpp"

#include "Shell/Options.hpp"
#include "Shell/Statistics.hpp"

#include "URResolution.hpp"

namespace Inferences
{

using namespace Lib;
using namespace Kernel;
using namespace Indexing;
using namespace Saturation;

void URResolution::attach(SaturationAlgorithm* salg)
{
  CALL("URResolution::attach");

  GeneratingInferenceEngine::attach(salg);

  _unitIndex = static_cast<UnitClauseLiteralIndex*> (
	  _salg->getIndexManager()->request(GENERATING_UNIT_CLAUSE_SUBST_TREE) );
  _nonUnitIndex = static_cast<NonUnitClauseLiteralIndex*> (
	  _salg->getIndexManager()->request(GENERATING_NON_UNIT_CLAUSE_SUBST_TREE) );
}

void URResolution::detach()
{
  CALL("URResolution::detach");

  _unitIndex = 0;
  _salg->getIndexManager()->release(GENERATING_UNIT_CLAUSE_SUBST_TREE);
  _nonUnitIndex = 0;
  _salg->getIndexManager()->release(GENERATING_NON_UNIT_CLAUSE_SUBST_TREE);
  GeneratingInferenceEngine::detach();
}

struct URResolution::Item
{
  Item(Clause* cl)
  : _mustResolveAll(cl->length() < 2), _orig(cl)
  {
    CALL("URResolution::Item::Item");

    unsigned clen = cl->length();
    _premises.init(clen, 0);
    _lits.initFromArray(clen, *cl);
  }

  /**
   * Resolve away @c idx -th literal of the clause. This involves
   * applying the substitution in @c unif to all remaining literals.
   * If @c useQuerySubstitution is true, the query part of the
   * substitution is applied to the literals, otherwise the result
   * part is applied.
   */
  void resolveLiteral(unsigned idx, SLQueryResult& unif, Clause* premise, bool useQuerySubstitution)
  {
    CALL("URResolution::Item::resolveLiteral");

    _lits[idx] = 0;
    _premises[idx] = premise;
    unsigned clen = _lits.size();
    for(unsigned i=0; i<clen; i++) {
      Literal*& lit = _lits[i];
      if(!lit) {
        continue;
      }
      if(useQuerySubstitution) {
	lit = unif.substitution->applyToQuery(lit);
      }
      else {
	lit = unif.substitution->applyToResult(lit);
      }
    }
  }

  Clause* generateClause() const
  {
    CALL("URResolution::Item::generateClause");

    UnitList* premLst = 0;
    UnitList::push(_orig, premLst);
    Literal* single = 0;
    unsigned clen = _lits.size();
    for(unsigned i=0; i<clen; i++) {
      if(_lits[i]!=0) {
	ASS_EQ(single,0);
	ASS_EQ(_premises[i],0);
	single = _lits[i];
      }
      else {
	ASS(_premises[i]);
	UnitList::push(_premises[i], premLst);
      }
    }
    Unit::InputType inpType = Unit::getInputType(premLst);
    Inference* inf = new InferenceMany(Inference::UNIT_RESULTING_RESOLUTION, premLst);
    Clause* res;
    if(single) {
      res = Clause::fromIterator(getSingletonIterator(single), inpType, inf);
    }
    else {
      res = Clause::fromIterator(LiteralIterator::getEmpty(), inpType, inf);
    }
    return res;
  }

  /** If true, we may skip resolving one of the remaining literals */
  bool _mustResolveAll;

  /** The original clause we are resolving */
  Clause* _orig;

  /** Premises used to resolve away particular literals */
  DArray<Clause*> _premises;

  /** Unresolved literals, or zeroes at positions of the resolved ones
   *
   * The unresolved literals have the substitutions from other resolutions
   * applied to themselves */
  DArray<Literal*> _lits;
};

/**
 * Perform one level of the BFS traversal of possible resolution
 * sequences
 *
 * (See documentation to the @c processAndGetClauses() function.)
 */
void URResolution::processLiteral(ItemList*& itms, unsigned idx)
{
  CALL("URResolution::processLiteral");

  ItemList::DelIterator iit(itms);
  while(iit.hasNext()) {
    Item* itm = iit.next();
    Literal* lit = itm->_lits[idx];
    ASS(lit);

    if(!itm->_mustResolveAll) {
      Item* itm2 = new Item(*itm);
      itm2->_mustResolveAll = true;
      iit.insert(itm2);
    }

    SLQueryResultIterator unifs = _unitIndex->getUnifications(lit, true, true);
    while(unifs.hasNext()) {
      SLQueryResult unif = unifs.next();

      Item* itm2 = new Item(*itm);
      itm2->resolveLiteral(idx, unif, unif.clause, true);
      iit.insert(itm2);
    }

    iit.del();
    delete itm;
  }
}

/**
 * Explore possible ways of resolving away literals in @c itm,
 * and from the successful ones add the resulting clause into
 * @c acc. The search starts at literal with index @c startIdx.
 *
 * What we do is a BFS traversal of all possible resolutions
 * on the clause represented in @c itm. In the @c itms list we
 * store all elements of @c i -th level of the search, and a
 * call to the @c processLiteral() function moves us to the next
 * level of the traversal.
 */
void URResolution::processAndGetClauses(Item* itm, unsigned startIdx, ClauseList*& acc)
{
  CALL("URResolution::processAndGetClauses");

  unsigned clen = itm->_lits.size();

  ItemList* itms = 0;
  ItemList::push(itm, itms);
  for(unsigned i = startIdx; itms && i<clen; i++) {
    processLiteral(itms, i);
  }

  while(itms) {
    Item* itm = ItemList::pop(itms);
    ClauseList::push(itm->generateClause(), acc);
    env.statistics->urResolution++;
    delete itm;
  }
}

/**
 * Perform URR inferences between a newly derived unit clause
 * @c cl and non-unit active clauses
 */
void URResolution::doBackwardInferences(Clause* cl, ClauseList*& acc)
{
  CALL("URResolution::doBackwardInferences");
  ASS_EQ(cl->size(), 1);

  Literal* lit = (*cl)[0];
  SLQueryResultIterator unifs = _nonUnitIndex->getUnifications(lit, true, true);
  while(unifs.hasNext()) {
    SLQueryResult unif = unifs.next();

    Clause* ucl = unif.clause;
    Item* itm = new Item(ucl);
    unsigned pos = ucl->getLiteralPosition(unif.literal);
    swap(itm->_lits[0], itm->_lits[pos]);
    itm->resolveLiteral(0, unif, cl, false);

    processAndGetClauses(itm, 1, acc);
  }
}

ClauseIterator URResolution::generateClauses(Clause* cl)
{
  CALL("URResolution::generateClauses");

  unsigned clen = cl->size();
  if(clen<1) {
    return ClauseIterator::getEmpty();
  }

  TimeCounter tc(TC_UR_RESOLUTION);

  ClauseList* res = 0;
  processAndGetClauses(new Item(cl), 0, res);

  if(clen==1) {
    doBackwardInferences(cl, res);
  }

  return getPersistentIterator(ClauseList::DestructiveIterator(res));
}

}
