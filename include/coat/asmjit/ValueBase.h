#ifndef COAT_ASMJIT_VALUEBASE_H_
#define COAT_ASMJIT_VALUEBASE_H_

#include <asmjit/asmjit.h>

//#include "Condition.h"


namespace coat {

template<>
struct ValueBase<::asmjit::x86::Compiler> {
	::asmjit::x86::Compiler &cc;
	::asmjit::x86::Gp reg;

	ValueBase(::asmjit::x86::Compiler &cc) : cc(cc) {}
	ValueBase(const ValueBase &other) : cc(other.cc), reg(other.reg) {}

	operator const ::asmjit::x86::Gp&() const { return reg; }
	operator       ::asmjit::x86::Gp&()       { return reg; }

#if 0
	ValueBase &operator=(const Condition<::asmjit::x86::Compiler> &cond){
		cond.compare();
		cond.setbyte(reg);
		return *this;
	}
#endif

	// identical operations for signed and unsigned, or different sizes
	// pre-increment, post-increment not supported as it leads to temporary
	ValueBase &operator++(){ cc.inc(reg); return *this; }
	// pre-decrement
	ValueBase &operator--(){ cc.dec(reg); return *this; }
};

} // namespace

#endif
