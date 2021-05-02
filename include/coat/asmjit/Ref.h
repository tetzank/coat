#ifndef COAT_ASMJIT_REF_H_
#define COAT_ASMJIT_REF_H_

#include "coat/Ref.h"
#include "operator_helper.h"

#include <asmjit/asmjit.h>

#include "Condition.h"


namespace coat {

//FIXME: looks like ::asmjit::x86::Mem is copied around quite often during construction
template<class T>
struct Ref<::asmjit::x86::Compiler,T> {
	using F = ::asmjit::x86::Compiler;
	using inner_type = T;

	::asmjit::x86::Compiler &cc; //FIXME: pointer stored in every value type
	::asmjit::x86::Mem mem;

	Ref(::asmjit::x86::Compiler &cc, ::asmjit::x86::Mem mem) : cc(cc), mem(mem) {}
	operator const ::asmjit::x86::Mem&() const { return mem; }
	operator       ::asmjit::x86::Mem&()       { return mem; }

	// assignment storing to memory
	Ref &operator=(const D<T> &other){
		cc.mov(mem, OP);
		DL;
		return *this;
	}
	Ref &operator=(const D<typename inner_type::value_type> &other){
		if constexpr(sizeof(typename inner_type::value_type) == 8){
			// mov to memory operand not allowed with imm64
			// copy immediate first to register, then store
			::asmjit::x86::Gp temp;
			if constexpr(std::is_signed_v<typename inner_type::value_type>){
				temp = cc.newInt64();
			}else{
				temp = cc.newUInt64();
			}
			cc.mov(temp, ::asmjit::imm(OP));
			DL;
			cc.mov(mem, temp);
			DL;
		}else{
			cc.mov(mem, ::asmjit::imm(OP));
			DL;
		}
		return *this;
	}
	// arithmetic + assignment skipped for now

	// operators creating temporary virtual registers
	ASMJIT_OPERATORS_WITH_TEMPORARIES(T)

	// comparisons
	// swap sides of operands and comparison, not needed for assembly, but avoids code duplication in wrapper
	Condition<F> operator==(const T &other) const { return other==*this; }
	Condition<F> operator!=(const T &other) const { return other!=*this; }
	Condition<F> operator< (const T &other) const { return other> *this; }
	Condition<F> operator<=(const T &other) const { return other>=*this; }
	Condition<F> operator> (const T &other) const { return other< *this; }
	Condition<F> operator>=(const T &other) const { return other<=*this; }
	//TODO: possible without temporary: cmp m32 imm32, complicates Condition
	Condition<F> operator==(int constant) const { T tmp(cc, "tmp"); tmp = *this; return tmp==constant; }
	Condition<F> operator!=(int constant) const { T tmp(cc, "tmp"); tmp = *this; return tmp!=constant; }
	Condition<F> operator< (int constant) const { T tmp(cc, "tmp"); tmp = *this; return tmp< constant; }
	Condition<F> operator<=(int constant) const { T tmp(cc, "tmp"); tmp = *this; return tmp<=constant; }
	Condition<F> operator> (int constant) const { T tmp(cc, "tmp"); tmp = *this; return tmp> constant; }
	Condition<F> operator>=(int constant) const { T tmp(cc, "tmp"); tmp = *this; return tmp>=constant; }
	// not possible in instruction, requires temporary
	Condition<F> operator==(const Ref &other) const { T tmp(cc, "tmp"); tmp = *this; return tmp==other; }
	Condition<F> operator!=(const Ref &other) const { T tmp(cc, "tmp"); tmp = *this; return tmp!=other; }
	Condition<F> operator< (const Ref &other) const { T tmp(cc, "tmp"); tmp = *this; return tmp< other; }
	Condition<F> operator<=(const Ref &other) const { T tmp(cc, "tmp"); tmp = *this; return tmp<=other; }
	Condition<F> operator> (const Ref &other) const { T tmp(cc, "tmp"); tmp = *this; return tmp> other; }
	Condition<F> operator>=(const Ref &other) const { T tmp(cc, "tmp"); tmp = *this; return tmp>=other; }
};

} // namespace

#endif
