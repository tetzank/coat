#ifndef COAT_OPERATOR_HELPER_H_
#define COAT_OPERATOR_HELPER_H_

#define OPERATOR_SYMBOLS(R,X) X(R,<<) X(R,>>) X(R,*) X(R,/) X(R,%) X(R,+) X(R,-) X(R,&) X(R,|) X(R,^)

#define OPERATOR_CONSTANT(R, OP) \
	R operator OP(int amount) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= amount; return tmp; }
#define OPERATOR_VALUE(R, OP) \
	R operator OP(const R &other) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= other; return tmp; }
#define OPERATOR_REF(R, OP) \
	R operator OP(const Ref<F,R> &other) const { R tmp(cc, "tmp"); tmp = *this; tmp OP##= other; return tmp; }

#define OPERATORS_WITH_TEMPORARIES(R) \
	OPERATOR_SYMBOLS(R, OPERATOR_CONSTANT) \
	OPERATOR_SYMBOLS(R, OPERATOR_VALUE) \
	OPERATOR_SYMBOLS(R, OPERATOR_REF)

#endif
