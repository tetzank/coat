#ifndef COAT_ASMJIT_VALUE_H_
#define COAT_ASMJIT_VALUE_H_


#include <type_traits>

#include <asmjit/asmjit.h>
#include "coat/runtimeasmjit.h"
#include "coat/constexpr_helper.h"

#include "DebugOperand.h"
#include "operator_helper.h"

#include "ValueBase.h"
//#include "Ref.h"
//#include "Ptr.h"

#include "coat/Label.h"


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
#ifdef PROFILING_SOURCE
	Value(F &cc, T val, const char *name="", const char *file=__builtin_FILE(), int line=__builtin_LINE()) : Value(cc, name) {
		*this = D<int>{val, file, line};
	}
#else
	Value(F &cc, T val, const char *name="") : Value(cc, name) {
		*this = val;
	}
#endif

	// real copy means new register and copy content
	Value(const Value &other) : Value(other.cc) {
		*this = other;
	}
	// move ctor
	Value(const Value &&other) : ValueBase(std::move(other)) {}
	// move assign
	Value &operator=(const Value &&other){
		reg = other.reg; // just take virtual register
		return *this;
	}

	// copy ctor for ref, basically loads value from memory and stores in register
	Value(const Ref<F,Value> &other) : Value(other.cc) {
		*this = other;
	}

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

	// explicit conversion, no assignment, new value/temporary
	//TODO: decide on one style and delete other
	template<typename O>
	Value<F,O> narrow(){
		static_assert(sizeof(O) < sizeof(T), "narrowing conversion called on wrong types");
		Value<F,O> tmp(cc, "narrowtmp");
		// copy from register
		switch(sizeof(O)){
			case 1: cc.mov(tmp.reg, reg.r8 ()); break;
			case 2: cc.mov(tmp.reg, reg.r16()); break;
			case 4: cc.mov(tmp.reg, reg.r32()); break;
		}
		return tmp;
	}
	template<typename O>
	Value<F,O> widen(){
		static_assert(sizeof(O) > sizeof(T), "widening conversion called on wrong types");
		Value<F,O> tmp(cc, "widentmp");
		if constexpr(std::is_signed_v<T>){
			if(sizeof(O)==8 && sizeof(T)==4){
				cc.movsxd(tmp.reg, reg);
			}else{
				cc.movsx(tmp.reg, reg);
			}
		}else{
			// movzx only from 8/16 to whatever, otherwise use normal move as it implicitly zero's upper 32-bit
			if(sizeof(T) <= 2){
				cc.movzx(tmp.reg, reg);
			}else{
				// write to 32-bit register, upper half zero'd
				cc.mov(tmp.reg.r32(), reg);
			}
		}
		return tmp;
	}

	// assignment
	Value &operator=(const D<Value> &other){
		cc.mov(reg, OP);
		DL;
		return *this;
	}
	Value &operator=(const D<int> &other){
		if(OP == 0){
			// just for fun, smaller opcode, writing 32-bit part zero's rest of 64-bit register too
			cc.xor_(reg.r32(), reg.r32());
			DL;
		}else{
			cc.mov(reg, ::asmjit::imm(OP));
			DL;
		}
		return *this;
	}
	Value &operator=(const D<Ref<F,Value>> &other){
		cc.mov(reg, OP);
		DL;
		return *this;
	}

	// special handling of bit tests, for convenience and performance
	void bit_test(const Value &bit, Label<F> &label, bool jump_on_set=true) const {
		cc.bt(reg, bit);
		if(jump_on_set){
			cc.jc(label);
		}else{
			cc.jnc(label);
		}
	}

	void bit_test_and_set(const Value &bit, Label<F> &label, bool jump_on_set=true){
		cc.bts(reg, bit);
		if(jump_on_set){
			cc.jc(label);
		}else{
			cc.jnc(label);
		}
	}

	void bit_test_and_reset(const Value &bit, Label<F> &label, bool jump_on_set=true){
		cc.btr(reg, bit);
		if(jump_on_set){
			cc.jc(label);
		}else{
			cc.jnc(label);
		}
	}

	// operators with assignment
	Value &operator<<=(const D<Value> &other){
		if constexpr(std::is_signed_v<T>){
			cc.sal(reg, OP);
		}else{
			cc.shl(reg, OP);
		}
		DL;
		return *this;
	}
	Value &operator<<=(const D<int> &other){
		if constexpr(std::is_signed_v<T>){
			cc.sal(reg, ::asmjit::imm(OP));
		}else{
			cc.shl(reg, ::asmjit::imm(OP));
		}
		DL;
		return *this;
	}
	// memory operand not possible on right side

	Value &operator>>=(const D<Value> &other){
		if constexpr(std::is_signed_v<T>){
			cc.sar(reg, OP);
		}else{
			cc.shr(reg, OP);
		}
		DL;
		return *this;
	}
	Value &operator>>=(const D<int> &other){
		if constexpr(std::is_signed_v<T>){
			cc.sar(reg, ::asmjit::imm(OP));
		}else{
			cc.shr(reg, ::asmjit::imm(OP));
		}
		DL;
		return *this;
	}
	// memory operand not possible on right side

	Value &operator*=(const D<Value> &other){
		static_assert(sizeof(T) > 1, "multiplication of byte type currently not supported");
		if constexpr(std::is_signed_v<T>){
			cc.imul(reg, OP);
		}else{
			::asmjit::x86::Gp r_upper = cc.newUInt64(); // don't care about type here
			switch(sizeof(T)){
				case 1: /* TODO */ break;
				case 2: cc.mul(r_upper.r16(), reg, OP); break;
				case 4: cc.mul(r_upper.r32(), reg, OP); break;
				case 8: cc.mul(r_upper.r64(), reg, OP); break;
			}
			// ignores upper part for now
		}
		DL;
		return *this;
	}
	// immediate not possible in mul, but imul has support
	Value &operator*=(const D<int> &other){
		//static_assert(sizeof(T) > 1, "multiplication of byte type currently not supported");
		// special handling of stuff which can be done with lea
		switch(OP){
			case  0: cc.xor_(reg, reg); break;
			case  1: /* do nothing */ break;
			case  2:
				// lea rax, [rax + rax]
				cc.lea(reg, ::asmjit::x86::ptr(reg, reg));
				DL;
				break;
			case  3:
				// lea rax, [rax + rax*2]
				cc.lea(reg, ::asmjit::x86::ptr(reg, reg, clog2(2)));
				DL;
				break;
			case  4:
				// lea rax, [0 + rax*4]
				cc.lea(reg, ::asmjit::x86::ptr(0, reg, clog2(4)));
				DL;
				break;
			case  5:
				// lea rax, [rax + rax*4]
				cc.lea(reg, ::asmjit::x86::ptr(reg, reg, clog2(4)));
				DL;
				break;
			case  6:
				// lea rax, [rax + rax*2]
				// add rax, rax
				cc.lea(reg, ::asmjit::x86::ptr(reg, reg, clog2(2)));
				DL;
				cc.add(reg, reg);
				DL;
				break;
			//case  7:
			//	// requires two registers
			//	// lea rbx, [0 + rax*8]
			//	// sub rbx, rax
			//	cc.lea(reg, ::asmjit::x86::ptr(0, reg, clog2(8)));
			//	cc.sub();
			//	break;
			case  8:
				// lea rax, [0 + rax*8]
				cc.lea(reg, ::asmjit::x86::ptr(0, reg, clog2(8)));
				DL;
				break;
			case  9:
				// lea rax, [rax + rax*8]
				cc.lea(reg, ::asmjit::x86::ptr(reg, reg, clog2(8)));
				DL;
				break;
			case 10:
				// lea rax, [rax + rax*4]
				// add rax, rax
				cc.lea(reg, ::asmjit::x86::ptr(reg, reg, clog2(4)));
				DL;
				cc.add(reg, reg);
				DL;
				break;

			default: {
				if(is_power_of_two(OP)){
					operator<<=(PASSOP(clog2(OP)));
				}else{
					if constexpr(std::is_signed_v<T>){
						cc.imul(reg, ::asmjit::imm(OP));
						DL;
					}else{
						Value temp(cc, T(OP), "constant");
						operator*=(PASSOP(temp));
					}
				}
			}
		}
		return *this;
	}

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
	Value &operator/=(int constant){
		if(is_power_of_two(constant)){
			operator>>=(clog2(constant));
		}else{
			Value temp(cc, T(constant), "constant");
			operator/=(temp);
		}
		return *this;
	}

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
	Value &operator%=(int constant){
		if(is_power_of_two(constant)){
			operator&=(constant - 1);
		}else{
			Value temp(cc, T(constant), "constant");
			operator%=(temp);
		}
		return *this;
	}

	Value &operator+=(const D<int>          &other){ cc.add(reg, ::asmjit::imm(OP)); DL; return *this; }
	Value &operator+=(const D<Value>        &other){ cc.add(reg, OP);                DL; return *this; }
	Value &operator+=(const D<Ref<F,Value>> &other){ cc.add(reg, OP);                DL; return *this; }

	Value &operator-=(const D<int>          &other){ cc.sub(reg, ::asmjit::imm(OP)); DL; return *this; }
	Value &operator-=(const D<Value>        &other){ cc.sub(reg, OP);                DL; return *this; }
	Value &operator-=(const D<Ref<F,Value>> &other){ cc.sub(reg, OP);                DL; return *this; }

	Value &operator&=(const D<int>          &other){ cc.and_(reg, ::asmjit::imm(OP)); DL; return *this; }
	Value &operator&=(const D<Value>        &other){ cc.and_(reg, OP);                DL; return *this; }
	Value &operator&=(const D<Ref<F,Value>> &other){ cc.and_(reg, OP);                DL; return *this; }

	Value &operator|=(const D<int>          &other){ cc.or_(reg, ::asmjit::imm(OP)); DL; return *this; }
	Value &operator|=(const D<Value>        &other){ cc.or_(reg, OP);                DL; return *this; }
	Value &operator|=(const D<Ref<F,Value>> &other){ cc.or_(reg, OP);                DL; return *this; }

	Value &operator^=(const D<int>          &other){ cc.xor_(reg, ::asmjit::imm(OP)); DL; return *this; }
	Value &operator^=(const D<Value>        &other){ cc.xor_(reg, OP);                DL; return *this; }
	Value &operator^=(const D<Ref<F,Value>> &other){ cc.xor_(reg, OP);                DL; return *this; }

	//TODO: cannot be attributed to a source line as we cannot pass a parameter
	//TODO: we do not know from where we are called
	//TODO: => use free standing function which has the object the unary operator applies to as parameter
	//FIXME: wrong, ~v returns new value, does not modify v, add new method bitwise_not() which does it inplace
	Value &operator~(){ cc.not_(reg); return *this; }

	// operators creating temporary virtual registers
	OPERATORS_WITH_TEMPORARIES(Value)

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
template<typename FnPtr, typename T> Value(Function<runtimeasmjit,FnPtr>&, T val, const char *) -> Value<::asmjit::x86::Compiler,T>;
template<typename T> Value(const Ref<::asmjit::x86::Compiler,Value<::asmjit::x86::Compiler,T>>&) -> Value<::asmjit::x86::Compiler,T>;

} // namespace

#endif
