#ifndef COAT_ASMJIT_OPERATOR_HELPER_H_
#define COAT_ASMJIT_OPERATOR_HELPER_H_

#include "DebugOperand.h"

//TODO: remove prefix ASMJIT_ when both backends have source line support, merge to one set of macros

#define ASMJIT_OPERATOR_SYMBOLS(R,X) X(R,<<) X(R,>>) X(R,*) X(R,/) X(R,%) X(R,+) X(R,-) X(R,&) X(R,|) X(R,^)


#ifdef PROFILING_SOURCE

#define ASMJIT_OPERATOR_CONSTANT(R, OP) \
	R operator OP(const D<int> &amount) const { R tmp(cc, "tmp"); tmp = DebugOperand{*this, amount.file, amount.line}; tmp OP##= amount; return tmp; }
#define ASMJIT_OPERATOR_VALUE(R, OP) \
	R operator OP(const D<R> &other) const { R tmp(cc, "tmp"); tmp = DebugOperand{*this, other.file, other.line}; tmp OP##= other; return tmp; }
#define ASMJIT_OPERATOR_REF(R, OP) \
	R operator OP(const D<Ref<F,R>> &other) const { R tmp(cc, "tmp"); tmp = DebugOperand{*this, other.file, other.line}; tmp OP##= other; return tmp; }

#else

#define ASMJIT_OPERATOR_CONSTANT(R, OP) \
	R operator OP(int amount) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= amount; return tmp; }
#define ASMJIT_OPERATOR_VALUE(R, OP) \
	R operator OP(const R &other) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= other; return tmp; }
#define ASMJIT_OPERATOR_REF(R, OP) \
	R operator OP(const Ref<F,R> &other) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= other; return tmp; }

#endif


#define ASMJIT_OPERATORS_WITH_TEMPORARIES(R) \
	ASMJIT_OPERATOR_SYMBOLS(R, ASMJIT_OPERATOR_CONSTANT) \
	ASMJIT_OPERATOR_SYMBOLS(R, ASMJIT_OPERATOR_VALUE) \
	ASMJIT_OPERATOR_SYMBOLS(R, ASMJIT_OPERATOR_REF)

#endif
