#ifndef COAT_ASMJIT_VALUE_H_
#define COAT_ASMJIT_VALUE_H_


#include <type_traits>

#include <asmjit/asmjit.h>
#include "../runtimeasmjit.h"
#include "../constexpr_helper.h"

#include "ValueBase.h"
//#include "Ref.h"
//#include "Ptr.h"


namespace coat {

template<typename T>
struct Value<::asmjit::x86::Compiler,T> final : public ValueBase<::asmjit::x86::Compiler> {
	using F = ::asmjit::x86::Compiler;
	using value_type = T;

	static_assert(sizeof(T)==1 || sizeof(T)==2 || sizeof(T)==4 || sizeof(T)==8,
		"only plain arithmetic types supported of sizes: 1, 2, 4 or 8 bytes");
	static_assert(std::is_signed_v<T> || std::is_unsigned_v<T>,
		"only plain signed or unsigned arithmetic types supported");

	Value(::asmjit::x86::Compiler &cc, const char *name="") : ValueBase(cc) {
		if constexpr(std::is_signed_v<T>){
			switch(sizeof(T)){
				case 1: reg = cc.newInt8 (name); break;
				case 2: reg = cc.newInt16(name); break;
				case 4: reg = cc.newInt32(name); break;
				case 8: reg = cc.newInt64(name); break;
			}
		}else{
			switch(sizeof(T)){
				case 1: reg = cc.newUInt8 (name); break;
				case 2: reg = cc.newUInt16(name); break;
				case 4: reg = cc.newUInt32(name); break;
				case 8: reg = cc.newUInt64(name); break;
			}
		}
	}
	Value(::asmjit::x86::Compiler &cc, T val, const char *name="") : ValueBase(cc) {
		if constexpr(std::is_signed_v<T>){
			switch(sizeof(T)){
				case 1: reg = cc.newInt8 (name); break;
				case 2: reg = cc.newInt16(name); break;
				case 4: reg = cc.newInt32(name); break;
				case 8: reg = cc.newInt64(name); break;
			}
		}else{
			switch(sizeof(T)){
				case 1: reg = cc.newUInt8 (name); break;
				case 2: reg = cc.newUInt16(name); break;
				case 4: reg = cc.newUInt32(name); break;
				case 8: reg = cc.newUInt64(name); break;
			}
		}
		*this = val;
	}

	Value(const Value &other) : ValueBase(other) {}

	// explicit type conversion, assignment
	// always makes a copy
	// FIXME: implicit conversion between signed and unsigned, but not between types of same size ...
	template<class O>
	Value &narrow(const O &other){
		if constexpr(std::is_base_of_v<ValueBase,O>){
			static_assert(sizeof(T) < sizeof(typename O::value_type), "narrowing conversion called on wrong types");
			// copy from register
			switch(sizeof(T)){
				case 1: cc.mov(reg, other.reg.r8 ()); break;
				case 2: cc.mov(reg, other.reg.r16()); break;
				case 4: cc.mov(reg, other.reg.r32()); break;
			}
		}else if(std::is_base_of_v<ValueBase,typename O::inner_type>){
			static_assert(sizeof(T) < sizeof(typename O::inner_type::value_type), "narrowing conversion called on wrong types");
			// memory operand
			switch(sizeof(T)){
				case 1: cc.mov(reg, other); break;
				case 2: cc.mov(reg, other); break;
				case 4: cc.mov(reg, other); break;
			}
		}else{
			static_assert(should_not_be_reached<O>, "assignment only allowed for wrapped register types");
		}
		return *this;
	}
	template<class O>
	Value &widen(const O &other){
		size_t other_size;
		if constexpr(std::is_base_of_v<ValueBase,O>){
			static_assert(sizeof(T) > sizeof(typename O::value_type), "widening conversion called on wrong types");
			other_size = sizeof(typename O::value_type);
		}else/* if(std::is_base_of_v<ValueBase,typename O::inner_type>)*/{ //FIXME
			static_assert(sizeof(T) > sizeof(typename O::inner_type::value_type), "widening conversion called on wrong types");
			other_size = sizeof(typename O::inner_type::value_type);
		}/*else{
			static_assert(should_not_be_reached<O>, "assignment only allowed for wrapped register types");
		}*/
		// syntax the same for register and memory operand, implicit conversion to reg or mem
		if constexpr(std::is_signed_v<T>){
			if(sizeof(T)==8 && other_size==4){
				cc.movsxd(reg, other);
			}else{
				cc.movsx(reg, other);
			}
		}else{
			// movzx only from 8/16 to whatever, otherwise use normal move as it implicitly zero's upper 32-bit
			if(other_size <= 2){
				cc.movzx(reg, other);
			}else{
				// write to 32-bit register, upper half zero'd
				cc.mov(reg.r32(), other);
			}
		}
		return *this;
	}

	// assignment
	Value &operator=(const Value &other){
		cc.mov(reg, other);
		return *this;
	}
	Value &operator=(int value){
		if(value == 0){
			// just for fun, smaller opcode, writing 32-bit part zero's rest of 64-bit register too
			cc.xor_(reg.r32(), reg.r32());
		}else{
			cc.mov(reg, ::asmjit::imm(value));
		}
		return *this;
	}
	Value &operator=(const Ref<F,Value> &other){
		cc.mov(reg, other);
		return *this;
	}

	Value &operator<<=(const Value &other){
		if constexpr(std::is_signed_v<T>){
			cc.sal(reg, other);
		}else{
			cc.shl(reg, other);
		}
		return *this;
	}
	Value &operator<<=(int amount){
		if constexpr(std::is_signed_v<T>){
			cc.sal(reg, ::asmjit::imm(amount));
		}else{
			cc.shl(reg, ::asmjit::imm(amount));
		}
		return *this;
	}
	// memory operand not possible on right side

	Value &operator>>=(const Value &other){
		if constexpr(std::is_signed_v<T>){
			cc.sar(reg, other);
		}else{
			cc.shr(reg, other);
		}
		return *this;
	}
	Value &operator>>=(int amount){
		if constexpr(std::is_signed_v<T>){
			cc.sar(reg, ::asmjit::imm(amount));
		}else{
			cc.shr(reg, ::asmjit::imm(amount));
		}
		return *this;
	}
	// memory operand not possible on right side

	Value &operator*=(const Value &other){
		static_assert(sizeof(T) > 1, "multiplication of byte type currently not supported");
		if constexpr(std::is_signed_v<T>){
			cc.imul(reg, other);
		}else{
			::asmjit::x86::Gp r_upper = cc.newUInt64(); // don't care about type here
			switch(sizeof(T)){
				case 1: /* TODO */ break;
				case 2: cc.mul(r_upper.r16(), reg, other); break;
				case 4: cc.mul(r_upper.r32(), reg, other); break;
				case 8: cc.mul(r_upper.r64(), reg, other); break;
			}
			// ignores upper part for now
		}
		return *this;
	}
	// immediate not possible in mul, but imul has support

	Value &operator/=(const Value &other){
		static_assert(sizeof(T) > 1, "division of byte type currently not supported");
		if constexpr(std::is_signed_v<T>){
			::asmjit::x86::Gp r_upper = cc.newUInt64(); // don't care about type here
			switch(sizeof(T)){
				case 1: /* TODO */ break;
				case 2: cc.cwd(r_upper.r16(), reg); cc.idiv(r_upper.r16(), reg, other); break;
				case 4: cc.cdq(r_upper.r32(), reg); cc.idiv(r_upper.r32(), reg, other); break;
				case 8: cc.cqo(r_upper.r64(), reg); cc.idiv(r_upper.r64(), reg, other); break;
			}
		}else{
			::asmjit::x86::Gp r_upper = cc.newUInt64(); // don't care about type here
			cc.xor_(r_upper.r32(), r_upper.r32());
			switch(sizeof(T)){
				case 1: /* TODO */ break;
				case 2: cc.div(r_upper.r16(), reg, other); break;
				case 4: cc.div(r_upper.r32(), reg, other); break;
				case 8: cc.div(r_upper.r64(), reg, other); break;
			}
		}
		return *this;
	}
	// immediate not possible in div or idiv

	Value &operator%=(const Value &other){
		static_assert(sizeof(T) > 1, "division of byte type currently not supported");
		if constexpr(std::is_signed_v<T>){
			::asmjit::x86::Gp r_upper;
			switch(sizeof(T)){
				case 1: /* TODO */ break;
				case 2: r_upper = cc.newInt16(); cc.cwd(r_upper, reg); break;
				case 4: r_upper = cc.newInt32(); cc.cdq(r_upper, reg); break;
				case 8: r_upper = cc.newInt64(); cc.cqo(r_upper, reg); break;
			}
			cc.idiv(r_upper, reg, other);
			// remainder in upper part, use this register from now on
			reg = r_upper;
		}else{
			::asmjit::x86::Gp r_upper;
			switch(sizeof(T)){
				case 1: /* TODO */ break;
				case 2: r_upper = cc.newUInt16(); break;
				case 4: r_upper = cc.newUInt32(); break;
				case 8: r_upper = cc.newUInt64(); break;
			}
			cc.xor_(r_upper.r32(), r_upper.r32());
			cc.div(r_upper, reg, other);
			// remainder in upper part, use this register from now on
			reg = r_upper;
		}
		return *this;
	}
	// immediate not possible in div or idiv

	Value &operator+=(const Value &other){      cc.add(reg, other); return *this; }
	Value &operator+=(int constant){            cc.add(reg, ::asmjit::imm(constant)); return *this; }
	Value &operator+=(const Ref<F,Value> &other){ cc.add(reg, other); return *this; }

	Value &operator-=(const Value &other){      cc.sub(reg, other); return *this; }
	Value &operator-=(int constant){            cc.sub(reg, ::asmjit::imm(constant)); return *this; }
	Value &operator-=(const Ref<F,Value> &other){ cc.sub(reg, other); return *this; }

	Value &operator&=(const Value &other){      cc.and_(reg, other); return *this; }
	Value &operator&=(int constant){            cc.and_(reg, ::asmjit::imm(constant)); return *this; }
	Value &operator&=(const Ref<F,Value> &other){ cc.and_(reg, other); return *this; }

	Value &operator|=(const Value &other){      cc.or_(reg, other); return *this; }
	Value &operator|=(int constant){            cc.or_(reg, ::asmjit::imm(constant)); return *this; }
	Value &operator|=(const Ref<F,Value> &other){ cc.or_(reg, other); return *this; }

	Value &operator^=(const Value &other){      cc.xor_(reg, other); return *this; }
	Value &operator^=(int constant){            cc.xor_(reg, ::asmjit::imm(constant)); return *this; }
	Value &operator^=(const Ref<F,Value> &other){ cc.xor_(reg, other); return *this; }

	Value &operator~(){ cc.not_(reg); return *this; }

	// operators creating temporary virtual registers
	Value operator<<(int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp <<= amount; return tmp; }
	Value operator>>(int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp >>= amount; return tmp; }
	Value operator+ (int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp  += amount; return tmp; }
	Value operator- (int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp  -= amount; return tmp; }
	Value operator& (int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp  &= amount; return tmp; }
	Value operator| (int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp  |= amount; return tmp; }
	Value operator^ (int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp  ^= amount; return tmp; }


	// comparisons
	Condition<F> operator==(const Value &other) const { return {cc, reg, other.reg, ConditionFlag::e};  }
	Condition<F> operator!=(const Value &other) const { return {cc, reg, other.reg, ConditionFlag::ne}; }
	Condition<F> operator< (const Value &other) const { return {cc, reg, other.reg, less()};  }
	Condition<F> operator<=(const Value &other) const { return {cc, reg, other.reg, less_equal()}; }
	Condition<F> operator> (const Value &other) const { return {cc, reg, other.reg, greater()};  }
	Condition<F> operator>=(const Value &other) const { return {cc, reg, other.reg, greater_equal()}; }
	Condition<F> operator==(int constant) const { return {cc, reg, constant, ConditionFlag::e};  }
	Condition<F> operator!=(int constant) const { return {cc, reg, constant, ConditionFlag::ne}; }
	Condition<F> operator< (int constant) const { return {cc, reg, constant, less()};  }
	Condition<F> operator<=(int constant) const { return {cc, reg, constant, less_equal()}; }
	Condition<F> operator> (int constant) const { return {cc, reg, constant, greater()};  }
	Condition<F> operator>=(int constant) const { return {cc, reg, constant, greater_equal()}; }
	Condition<F> operator==(const Ref<F,Value> &other) const { return {cc, reg, other.mem, ConditionFlag::e};  }
	Condition<F> operator!=(const Ref<F,Value> &other) const { return {cc, reg, other.mem, ConditionFlag::ne}; }
	Condition<F> operator< (const Ref<F,Value> &other) const { return {cc, reg, other.mem, less()};  }
	Condition<F> operator<=(const Ref<F,Value> &other) const { return {cc, reg, other.mem, less_equal()}; }
	Condition<F> operator> (const Ref<F,Value> &other) const { return {cc, reg, other.mem, greater()};  }
	Condition<F> operator>=(const Ref<F,Value> &other) const { return {cc, reg, other.mem, greater_equal()}; }

	static inline constexpr ConditionFlag less(){
		if constexpr(std::is_signed_v<T>) return ConditionFlag::l;
		else                              return ConditionFlag::b;
	}
	static inline constexpr ConditionFlag less_equal(){
		if constexpr(std::is_signed_v<T>) return ConditionFlag::le;
		else                              return ConditionFlag::be;
	}
	static inline constexpr ConditionFlag greater(){
		if constexpr(std::is_signed_v<T>) return ConditionFlag::g;
		else                              return ConditionFlag::a;
	}
	static inline constexpr ConditionFlag greater_equal(){
		if constexpr(std::is_signed_v<T>) return ConditionFlag::ge;
		else                              return ConditionFlag::ae;
	}
};

// deduction guides
template<typename T> Value(::asmjit::x86::Compiler&, T val, const char *) -> Value<::asmjit::x86::Compiler,T>;
template<typename FnPtr, typename T> Value(Function<runtimeAsmjit,FnPtr>&, T val, const char *) -> Value<::asmjit::x86::Compiler,T>;

} // namespace

#endif
