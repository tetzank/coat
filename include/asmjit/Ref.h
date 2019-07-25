#ifndef COAT_ASMJIT_REF_H_
#define COAT_ASMJIT_REF_H_

#include <asmjit/asmjit.h>


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
	Ref &operator=(const T &other){
		cc.mov(mem, other);
		return *this;
	}
	Ref &operator=(int value){
		cc.mov(mem, ::asmjit::imm(value));
		return *this;
	}
	// arithmetic + assignment skipped for now

	// operators creating temporary virtual registers
	T operator<<(int amount) const { T tmp(cc, "tmp"); tmp = *this; tmp <<= amount; return tmp; }
	T operator>>(int amount) const { T tmp(cc, "tmp"); tmp = *this; tmp >>= amount; return tmp; }
	T operator+ (int amount) const { T tmp(cc, "tmp"); tmp = *this; tmp  += amount; return tmp; }
	T operator- (int amount) const { T tmp(cc, "tmp"); tmp = *this; tmp  -= amount; return tmp; }
	T operator& (int amount) const { T tmp(cc, "tmp"); tmp = *this; tmp  &= amount; return tmp; }
	T operator| (int amount) const { T tmp(cc, "tmp"); tmp = *this; tmp  |= amount; return tmp; }
	T operator^ (int amount) const { T tmp(cc, "tmp"); tmp = *this; tmp  ^= amount; return tmp; }

	T operator*(const T &other) const { T tmp(cc, "tmp"); tmp = *this; tmp *= other; return tmp; }
	T operator/(const T &other) const { T tmp(cc, "tmp"); tmp = *this; tmp /= other; return tmp; }
	T operator%(const T &other) const { T tmp(cc, "tmp"); tmp = *this; tmp %= other; return tmp; }


	// comparisons
	// swap sides of operands and comparison, not needed for assembly, but avoids code duplication in wrapper
	Condition<F> operator==(const T &other) const { return other==*this; }
	Condition<F> operator!=(const T &other) const { return other!=*this; }
	Condition<F> operator< (const T &other) const { return other>=*this; }
	Condition<F> operator<=(const T &other) const { return other> *this; }
	Condition<F> operator> (const T &other) const { return other<=*this; }
	Condition<F> operator>=(const T &other) const { return other< *this; }
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
