/**
 * @file LiteralComparators.hpp
 * Defines namespace Kernel::LiteralComparators containing atomic
 * literal comparator classes.
 */

#ifndef __CompositeLiteralComparator__
#define __CompositeLiteralComparator__

#include "../Lib/Comparison.hpp"
#include "../Lib/Int.hpp"

#include "LiteralSelector.hpp"
#include "Term.hpp"

namespace Kernel {
namespace LiteralComparators {

using namespace Lib;



template<class Comp1, class Comp2>
class Composite
{
public:
  Comparison compare(Literal* l1, Literal* l2)
  {
    Comparison res1=_c1.compare(l1,l2);
    return (res1==EQUAL)?_c2.compare(l1,l2):res1;
  }
private:
  Comp1 _c1;
  Comp2 _c2;
};

template<class Comp>
class Inverse
{
public:
  Comparison compare(Literal* l1, Literal* l2)
  {
    return _c.compare(l2,l1);
  }
private:
  Comp _c;
};

struct ColoredFirst
{
  Comparison compare(Literal* l1, Literal* l2)
  {
    if(l1->color()!=COLOR_TRANSPARENT && l2->color()==COLOR_TRANSPARENT) {
      return GREATER;
    } else if(l1->color()==COLOR_TRANSPARENT && l2->color()!=COLOR_TRANSPARENT) {
      return LESS;
    } else {
      return EQUAL;
    }
  }
};

struct NoPositiveEquality
{
  Comparison compare(Literal* l1, Literal* l2)
  {
    bool l1PE=l1->isEquality()&&l1->isPositive();
    bool l2PE=l2->isEquality()&&l2->isPositive();
    if( l1PE && !l2PE ) {
      return LESS;
    } else if( !l1PE && l2PE ) {
      return GREATER;
    } else {
      return EQUAL;
    }
  }
};

struct Negative
{
  Comparison compare(Literal* l1, Literal* l2)
  {
    bool l1N=l1->isNegative();
    bool l2N=l2->isNegative();
    if( l1N && !l2N ) {
      return GREATER;
    } else if( !l1N && l2N ) {
      return LESS;
    } else {
      return EQUAL;
    }
  }
};

struct NegativeEquality
{
  Comparison compare(Literal* l1, Literal* l2)
  {
    bool l1NE=l1->isEquality()&&l1->isNegative();
    bool l2NE=l2->isEquality()&&l2->isNegative();
    if( l1NE && !l2NE ) {
      return GREATER;
    } else if( !l1NE && l2NE ) {
      return LESS;
    } else {
      return EQUAL;
    }
  }
};

struct MaximalSize
{
  Comparison compare(Literal* l1, Literal* l2)
  {
    return Int::compare(l1->weight(), l2->weight());
  }
};

struct LeastVariables
{
  Comparison compare(Literal* l1, Literal* l2)
  {
    return Int::compare(l2->vars(), l1->vars());
  }
};

struct LeastDistinctVariables
{
  Comparison compare(Literal* l1, Literal* l2)
  {
    return Int::compare(l2->getDistinctVars(), l1->getDistinctVars());
  }
};

struct LeastTopLevelVariables
{
  Comparison compare(Literal* l1, Literal* l2)
  {
    return Int::compare(getTLVarCnt(l2), getTLVarCnt(l1));
  }
private:
  unsigned getTLVarCnt(Literal* l)
  {
    unsigned res=0;
    for(TermList* arg=l->args(); arg->isNonEmpty(); arg=arg->next()) {
      if(arg->isVar()) {
	res++;
      }
    }
    return res;
  }
};

struct LexComparator
{
  Comparison compare(Literal* l1, Literal* l2)
  {
    ASS(l1->shared());
    ASS(l2->shared());

    if(l1->header()!=l2->header()) {
      return Int::compare(l1->header(),l2->header());
    }

    Term::SubtermIterator sit1(l1);
    Term::SubtermIterator sit2(l2);
    while(sit1.hasNext()) {
      ALWAYS(sit2.hasNext());
      TermList st1=sit1.next();
      TermList st2=sit2.next();
      if(st1.isTerm()) {
	if(st2.isTerm()) {
	  unsigned f1=st1.term()->functor();
	  unsigned f2=st2.term()->functor();
	  if(f1!=f2) {
	    return Int::compare(f1,f2);
	  }
	} else {
	  return GREATER;
	}
      } else {
	if(st2.isTerm()) {
	  return LESS;
	} else {
	  if(st1.var()!=st2.var()) {
	    return Int::compare(st1.var(),st2.var());
	  }
	}
      }
    }
    ASS(l1==l2);
    return EQUAL;
  }
};


/**
 * Literal ordering in which @b l1 is equal to @b l2
 * iff they're variants of each other, and if one literal
 * heavier than the other one, it is greater
 */
template<bool ignorePolarity=false>
struct NormalizedLinearComparatorByWeight
{
  Comparison compare(Term* t1, Term* t2)
  {
    ASS_EQ(t1->isLiteral(), t2->isLiteral());

    if(t1->weight()!=t2->weight()) {
      return Int::compare(t1->weight(),t2->weight());
    }
    if(t1->functor()!=t2->functor()) {
      return Int::compare(t1->functor(),t2->functor());
    }
    if(t1->isLiteral() && !ignorePolarity &&
	    static_cast<Literal*>(t1)->polarity()!=static_cast<Literal*>(t2)->polarity()) {
      return Int::compare(static_cast<Literal*>(t1)->polarity(),
	      static_cast<Literal*>(t2)->polarity());
    }

    if(false && t1->commutative()) {
      ASS(t2->commutative());
      ASS_EQ(t1->arity(),2);

      //TODO: doesn't work for p(X)=p(X) and p(X)=p(Y)

      NOT_IMPLEMENTED;

      TermList t1f=*t1->nthArgument(0);
      TermList t1s=*t1->nthArgument(1);
      TermList t2f=*t2->nthArgument(0);
      TermList t2s=*t2->nthArgument(1);

      //we can call compare recurrently, as we haven't reached any
      //static variables yet
      bool t1FirstGreater=compare(t1f, t1s);
      if(compare(t1f, t1s)==LESS) {
	swap(t1f, t1s);
      }
      if(compare(t2f, t2s)==LESS) {
	swap(t2f, t2s);
      }
      Comparison res=compare(t1f, t2f);
      if(res==EQUAL) {
	res=compare(t1s,t2s);
      }
      return res;
    }

    static DHMap<unsigned, unsigned> firstNums;
    static DHMap<unsigned, unsigned> secondNums;
    firstNums.reset();
    secondNums.reset();

    Term::DisagreementSetIterator dsit(t1,t2,true);
    while(dsit.hasNext()) {
      pair<TermList, TermList> dis=dsit.next();
      if(dis.first.isTerm()) {
	if(dis.second.isTerm()) {
	  ASS_NEQ(dis.first.term()->functor(), dis.second.term()->functor());
	  return Int::compare(dis.first.term()->functor(), dis.second.term()->functor());
	}
	return GREATER;
      }
      if(dis.second.isTerm()) {
	return LESS;
      }
      int firstNorm=firstNums.findOrInsert(dis.first.var(), firstNums.size());
      int secondNorm=secondNums.findOrInsert(dis.second.var(), secondNums.size());
      if(firstNorm!=secondNorm) {
	return Int::compare(secondNorm, firstNorm);
      }
    }
    //they're variants of each other
    return EQUAL;
  }

  Comparison compare(TermList t1, TermList t2)
  {
    NOT_IMPLEMENTED;
    if(t1.isVar()) {
      if(t2.isVar()) {

      }
    }
  }

};



}
}

#endif /* __CompositeLiteralComparator__ */
