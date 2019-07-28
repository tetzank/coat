#ifndef COAT_LLVMJIT_VALUE_H_
#define COAT_LLVMJIT_VALUE_H_

#include <llvm/IR/IRBuilder.h>

#include "ValueBase.h"
//#include "Condition.h"
//#include "Ref.h"
//#include "Ptr.h"

namespace coat {

template<typename T>
struct Value<llvm::IRBuilder<>,T> final : public ValueBase<llvm::IRBuilder<>> {
	using F = ::llvm::IRBuilder<>;
	using value_type = T;

	static_assert(sizeof(T)==1 || sizeof(T)==2 || sizeof(T)==4 || sizeof(T)==8,
		"only plain arithmetic types supported of sizes: 1, 2, 4 or 8 bytes");
	static_assert(std::is_signed_v<T> || std::is_unsigned_v<T>,
		"only plain signed or unsigned arithmetic types supported");

	Value(llvm::IRBuilder<> &cc, const char *name="") : ValueBase(cc) {
		// llvm IR has no types for unsigned/signed integers
		switch(sizeof(T)){
			case 1: memreg = cc.CreateAlloca(llvm::Type::getInt8Ty (cc.getContext()), nullptr, name); break;
			case 2: memreg = cc.CreateAlloca(llvm::Type::getInt16Ty(cc.getContext()), nullptr, name); break;
			case 4: memreg = cc.CreateAlloca(llvm::Type::getInt32Ty(cc.getContext()), nullptr, name); break;
			case 8: memreg = cc.CreateAlloca(llvm::Type::getInt64Ty(cc.getContext()), nullptr, name); break;
		}
	}
	Value(llvm::IRBuilder<> &cc, T val, const char *name="") : ValueBase(cc) {
		// llvm IR has no types for unsigned/signed integers
		switch(sizeof(T)){
			case 1: memreg = cc.CreateAlloca(llvm::Type::getInt8Ty (cc.getContext()), nullptr, name); break;
			case 2: memreg = cc.CreateAlloca(llvm::Type::getInt16Ty(cc.getContext()), nullptr, name); break;
			case 4: memreg = cc.CreateAlloca(llvm::Type::getInt32Ty(cc.getContext()), nullptr, name); break;
			case 8: memreg = cc.CreateAlloca(llvm::Type::getInt64Ty(cc.getContext()), nullptr, name); break;
		}
		*this = val;
	}

	Value(const Value &other) : ValueBase(other) {}

	// explicit type conversion, assignment
	// always makes a copy
	// FIXME: implicit conversion between signed and unsigned, but not between types of same size ...
	template<class O>
	Value &narrow(const O &other){
		static_assert(sizeof(T) < sizeof(typename O::value_type), "narrowing conversion called on wrong types");
		store( cc.CreateTrunc(other.load(), type()) );
		return *this;
	}

	template<class O>
	Value &widen(const O &other){
		if constexpr(std::is_base_of_v<ValueBase,O>){
			static_assert(sizeof(T) > sizeof(typename O::value_type), "widening conversion called on wrong types");
		}else{
			static_assert(sizeof(T) > sizeof(typename O::inner_type::value_type), "widening conversion called on wrong types");
		}
		if constexpr(std::is_signed_v<T>){
			store( cc.CreateSExt(other.load(), type()) );
		}else{
			store( cc.CreateZExt(other.load(), type()) );
		}
		return *this;
	}

	// assignment
	Value &operator=(const Value &other){ store( other.load() ); return *this; }
	Value &operator=(int value){ store( llvm::ConstantInt::get(type(), value) ); return *this; }
	Value &operator=(const Ref<F,Value> &other){ store( other.load() ); return *this; }
	//FIXME: takes any type
	Value &operator=(llvm::Value *val){ store( val ); return *this; }

	Value &operator<<=(const Value &other){ store( cc.CreateShl(load(), other.load()) ); return *this; }
	Value &operator<<=(int amount){ store( cc.CreateShl(load(), amount) ); return *this; }
	// memory operand not possible on right side

	Value &operator>>=(const Value &other){
		if constexpr(std::is_signed_v<T>){
			store( cc.CreateAShr(load(), other.load()) );
		}else{
			store( cc.CreateLShr(load(), other.load()) );
		}
		return *this;
	}
	Value &operator>>=(int amount){
		if constexpr(std::is_signed_v<T>){
			store( cc.CreateAShr(load(), amount) );
		}else{
			store( cc.CreateLShr(load(), amount) );
		}
		return *this;
	}
	// memory operand not possible on right side

	//TODO: *=, /= %=

	Value &operator+=(const Value &other){ store( cc.CreateAdd(load(), other.load()) ); return *this; }
	Value &operator+=(int constant){ store( cc.CreateAdd(load(), llvm::ConstantInt::get(type(),constant)) ); return *this; }
	Value &operator+=(const Ref<F,Value> &other){ store( cc.CreateAdd(load(), other.load()) ); return *this; }

	Value &operator-=(const Value &other){ store( cc.CreateSub(load(), other.load()) ); return *this; }
	Value &operator-=(int constant){ store( cc.CreateSub(load(), llvm::ConstantInt::get(type(),constant)) ); return *this; }
	Value &operator-=(const Ref<F,Value> &other){ store( cc.CreateSub(load(), other.load()) ); return *this; }

	Value &operator&=(const Value &other){          store( cc.CreateAnd(load(), other.load()) ); return *this; }
	Value &operator&=(int constant){               store( cc.CreateAnd(load(), constant) ); return *this; }
	//Value &operator&=(const Ref<Value> &other){ cc.and_(reg, other); return *this; }

	Value &operator|=(const Value &other){          store( cc.CreateOr(load(), other.load()) ); return *this; }
	Value &operator|=(int constant){               store( cc.CreateOr(load(), constant) ); return *this; }
	//Value &operator|=(const Ref<Value> &other){ cc.or_(reg, other); return *this; }

	Value &operator^=(const Value &other){          store( cc.CreateXor(load(), other.load()) ); return *this; }
	Value &operator^=(int constant){               store( cc.CreateXor(load(), constant) ); return *this; }
	//Value &operator^=(const Ref<Value> &other){ cc.xor_(reg, other); return *this; }

	//Value &operator~(){ cc.not_(reg); return *this; }

	// operators creatting temporary
	Value operator<<(int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp <<= amount; return tmp; }
	Value operator>>(int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp >>= amount; return tmp; }
	Value operator+ (int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp  += amount; return tmp; }
	Value operator- (int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp  -= amount; return tmp; }
	Value operator& (int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp  &= amount; return tmp; }
	Value operator| (int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp  |= amount; return tmp; }
	Value operator^ (int amount) const { Value tmp(cc, "tmp"); tmp = *this; tmp  ^= amount; return tmp; }


	// comparisons
	Condition<F> operator==(const Value &other) const { return {cc, memreg, other.memreg, ConditionFlag::e};  }
	Condition<F> operator!=(const Value &other) const { return {cc, memreg, other.memreg, ConditionFlag::ne}; }
	Condition<F> operator< (const Value &other) const { return {cc, memreg, other.memreg, less()};  }
	Condition<F> operator<=(const Value &other) const { return {cc, memreg, other.memreg, less_equal()}; }
	Condition<F> operator> (const Value &other) const { return {cc, memreg, other.memreg, greater()};  }
	Condition<F> operator>=(const Value &other) const { return {cc, memreg, other.memreg, greater_equal()}; }
	Condition<F> operator==(int constant) const { return {cc, memreg, constant, ConditionFlag::e};  }
	Condition<F> operator!=(int constant) const { return {cc, memreg, constant, ConditionFlag::ne}; }
	Condition<F> operator< (int constant) const { return {cc, memreg, constant, less()};  }
	Condition<F> operator<=(int constant) const { return {cc, memreg, constant, less_equal()}; }
	Condition<F> operator> (int constant) const { return {cc, memreg, constant, greater()};  }
	Condition<F> operator>=(int constant) const { return {cc, memreg, constant, greater_equal()}; }
	Condition<F> operator==(const Ref<F,Value> &other) const { return {cc, memreg, other.mem, ConditionFlag::e};  }
	Condition<F> operator!=(const Ref<F,Value> &other) const { return {cc, memreg, other.mem, ConditionFlag::ne}; }
	Condition<F> operator< (const Ref<F,Value> &other) const { return {cc, memreg, other.mem, less()};  }
	Condition<F> operator<=(const Ref<F,Value> &other) const { return {cc, memreg, other.mem, less_equal()}; }
	Condition<F> operator> (const Ref<F,Value> &other) const { return {cc, memreg, other.mem, greater()};  }
	Condition<F> operator>=(const Ref<F,Value> &other) const { return {cc, memreg, other.mem, greater_equal()}; }

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
template<typename T> Value(::llvm::IRBuilder<>&, T val, const char *) -> Value<::llvm::IRBuilder<>,T>;
template<typename FnPtr, typename T> Value(Function<runtimellvmjit,FnPtr>&, T val, const char *) -> Value<::llvm::IRBuilder<>,T>;

} // namespace

#endif
