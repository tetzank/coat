#ifndef CONDITION_H_
#define CONDITION_H_


enum class ConditionFlag {
	e, ne,
	//z, nz,
	l, le, b, be,
	g, ge, a, ae,
};


template<class CC>
struct Condition;

#ifdef ENABLE_ASMJIT
#  include "asmjit/Condition.h"
#endif

#ifdef ENABLE_LLVMJIT
#  include "llvmjit/Condition.h"
#endif


#endif
