/*
 * This file is part of the source code of the software program
 * Vampire. It is protected by applicable
 * copyright laws.
 *
 * This source code is distributed under the licence found here
 * https://vprover.github.io/license.html
 * and in the source directory
 */
/**
 * @file SubsumptionRemover.hpp
 * Defines class SubsumptionRemover.
 */

#ifndef __SubsumptionRemover__
#define __SubsumptionRemover__
#if GNUMP
#include "Forwards.hpp"

namespace Shell {

using namespace Lib;
using namespace Kernel;

class SubsumptionRemover {
public:
  bool apply(ConstraintRCList*& lst);
private:
  struct CoeffArrayHash;

  bool firstIsSubsumed(Constraint& c1, Constraint& c2);
};

}

#endif //GNUMP
#endif // __SubsumptionRemover__
