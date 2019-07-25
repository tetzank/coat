#ifndef COAT_CONDITION_H_
#define COAT_CONDITION_H_

namespace coat {

enum class ConditionFlag {
	e, ne,
	//z, nz,
	l, le, b, be,
	g, ge, a, ae,
};


template<class CC>
struct Condition;

} // namespace

#ifdef ENABLE_ASMJIT
#  include "asmjit/Condition.h"
#endif

#ifdef ENABLE_LLVMJIT
#  include "llvmjit/Condition.h"
#endif


#endif
