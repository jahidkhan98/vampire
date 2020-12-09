/*
 * File BiMap.hpp.
 *
 * This file is part of the source code of the software program
 * Vampire. It is protected by applicable
 * copyright laws.
 *
 * This source code is distributed under the licence found here
 * https://vprover.github.io/license.html
 * and in the source directory
 */

/** 
 * Defines a bi-directional HashMap. This data structure is just a convenient abstraction using two `Map`s under the hood.
 */

#ifndef __LIB__BI_MAP__HPP__
#define __LIB__BI_MAP__HPP__

#include "Lib/Map.hpp"

namespace Lib{

/**
 * A bidirectional hash map, implemented using two @c Map s under the hood. 
 * The methods behave the same as their counterparts in @c Map, with the exception that BiMap
 * asserts that every key, as well as every value is unique in this map (which is necessary
 * to do a bijective mapping.)
 */
template<class A, class B, class HashA = Lib::Hash, class HashB = Lib::Hash>
class BiMap : Map<A,B, HashA>, Map<B, A, HashB> {
  // Map<A,B> _to;
  // Map<B,A> _from;
  using Into = Map<A,B,HashA>;
  using From = Map<B,A,HashB>;
public:

  /** @see Map::get */
  using From::get;

  /** @see Map::get */
  using Into::get;

  /** @see Map::tryGet */
  using From::tryGet;

  /** @see Map::tryGet */
  using Into::tryGet;

  /** @see Map::find */
  using From::find;

  /** @see Map::find */
  using Into::find;

 
  /** @see Map::getOrInit */
  template<class InitFn>
  B& getOrInit(A key, InitFn init) 
  {
    CALL("Map::getOrInit");
    return Into::getOrInit(key, [&]() {
        auto val = init();
        From::insert(val, key);
        return std::move(val);
    });
  } 

  /** @see Map::clear */
  void clear() 
  {
    From::clear();
    Into::clear();
  }

  /** 
   * @see Map::insert 
   * @pre Asserts that both key and value do not yet exist in this BiMap.
   */
  inline void insert(A key, B val)
  {
    ASS(!find(key))
    ASS(!find(val))
    From::insert(val, key);
    Into::insert(key, val);
  }
};

} // namespace Lib
#endif // __LIB__BI_MAP__HPP__