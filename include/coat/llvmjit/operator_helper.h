#ifndef COAT_LLVMJIT_OPERATOR_HELPER_H_
#define COAT_LLVMJIT_OPERATOR_HELPER_H_

//TODO: remove prefix LLVMJIT_ when both backends have source line support, merge to one set of macros

#define LLVMJIT_OPERATOR_SYMBOLS(R,X) X(R,<<) X(R,>>) X(R,*) X(R,/) X(R,%) X(R,+) X(R,-) X(R,&) X(R,|) X(R,^)

#define LLVMJIT_OPERATOR_CONSTANT(R, OP) \
	R operator OP(int amount) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= amount; return tmp; }
#define LLVMJIT_OPERATOR_VALUE(R, OP) \
	R operator OP(const R &other) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= other; return tmp; }
#define LLVMJIT_OPERATOR_REF(R, OP) \
	R operator OP(const Ref<F,R> &other) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= other; return tmp; }

#define LLVMJIT_OPERATORS_WITH_TEMPORARIES(R) \
	LLVMJIT_OPERATOR_SYMBOLS(R, LLVMJIT_OPERATOR_CONSTANT) \
	LLVMJIT_OPERATOR_SYMBOLS(R, LLVMJIT_OPERATOR_VALUE) \
	LLVMJIT_OPERATOR_SYMBOLS(R, LLVMJIT_OPERATOR_REF)

#endif
