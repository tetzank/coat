#ifndef COAT_ASMJIT_OPERATOR_HELPER_H_
#define COAT_ASMJIT_OPERATOR_HELPER_H_

#include "DebugOperand.h"

#undef OPERATOR_SYMBOLS
#undef OPERATOR_CONSTANT
#undef OPERATOR_VALUE
#undef OPERATOR_REF
#undef OPERATORS_WITH_TEMPORARIES

#define OPERATOR_SYMBOLS(R,X) X(R,<<) X(R,>>) X(R,*) X(R,/) X(R,%) X(R,+) X(R,-) X(R,&) X(R,|) X(R,^)


#ifdef PROFILING_SOURCE

#define OPERATOR_CONSTANT(R, OP) \
	R operator OP(const D<int> &amount) const { R tmp(cc, "tmp"); tmp = D<R>{*this, amount.file, amount.line}; tmp OP##= amount; return tmp; }
#define OPERATOR_VALUE(R, OP) \
	R operator OP(const D<R> &other) const { R tmp(cc, "tmp"); tmp = D<R>{*this, other.file, other.line}; tmp OP##= other; return tmp; }
#define OPERATOR_REF(R, OP) \
	R operator OP(const D<Ref<F,R>> &other) const { R tmp(cc, "tmp"); tmp = D<R>{*this, other.file, other.line}; tmp OP##= other; return tmp; }

#else

#define OPERATOR_CONSTANT(R, OP) \
	R operator OP(int amount) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= amount; return tmp; }
#define OPERATOR_VALUE(R, OP) \
	R operator OP(const R &other) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= other; return tmp; }
#define OPERATOR_REF(R, OP) \
	R operator OP(const Ref<F,R> &other) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= other; return tmp; }

#endif


#define OPERATORS_WITH_TEMPORARIES(R) \
	OPERATOR_SYMBOLS(R, OPERATOR_CONSTANT) \
	OPERATOR_SYMBOLS(R, OPERATOR_VALUE) \
	OPERATOR_SYMBOLS(R, OPERATOR_REF)

#endif
