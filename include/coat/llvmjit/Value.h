#ifndef COAT_LLVMJIT_VALUE_H_
#define COAT_LLVMJIT_VALUE_H_

#include "ValueBase.h"
//#include "Condition.h"
//#include "Ref.h"
//#include "Ptr.h"

#include "coat/Label.h"

#include "operator_helper.h"


namespace coat {

template<typename T>
struct Value<LLVMBuilders,T> final : public ValueBase<LLVMBuilders> {
	using F = LLVMBuilders;
	using value_type = T;

	static_assert(sizeof(T)==1 || sizeof(T)==2 || sizeof(T)==4 || sizeof(T)==8,
		"only plain arithmetic types supported of sizes: 1, 2, 4 or 8 bytes");
	static_assert(std::is_signed_v<T> || std::is_unsigned_v<T>,
		"only plain signed or unsigned arithmetic types supported");

	Value(F &cc, const char *name=""
#ifdef LLVMJIT_DEBUG
			,bool isParameter=false,
			const char *file=__builtin_FILE(),
			int line=__builtin_LINE()
#endif
	) : ValueBase(cc) {
		// llvm IR has no types for unsigned/signed integers
		switch(sizeof(T)){
			case 1: memreg = allocateStackVariable(cc.ir, llvm::Type::getInt8Ty (cc.ir.getContext()), name); break;
			case 2: memreg = allocateStackVariable(cc.ir, llvm::Type::getInt16Ty(cc.ir.getContext()), name); break;
			case 4: memreg = allocateStackVariable(cc.ir, llvm::Type::getInt32Ty(cc.ir.getContext()), name); break;
			case 8: memreg = allocateStackVariable(cc.ir, llvm::Type::getInt64Ty(cc.ir.getContext()), name); break;
		}
#ifdef LLVMJIT_DEBUG
		// debug information
		llvm::DILocalVariable *di_var;
		//TODO: file?
		if(isParameter){
			//TODO: param number
			di_var = cc.dbg.createParameterVariable(cc.debugScope, name, 0, cc.debugScope->getFile(), line, getDebugType<value_type>(cc.dbg, cc.debugScope), true); //TODO: why alwaysPreserve=true?
		}else{
			di_var = cc.dbg.createAutoVariable(cc.debugScope, name, cc.debugScope->getFile(), line, getDebugType<value_type>(cc.dbg, cc.debugScope));
		}
		cc.dbg.insertDeclare(memreg, di_var, cc.dbg.createExpression(), llvm::DebugLoc::get(line, 0, cc.debugScope), cc.ir.GetInsertBlock());
#endif
	}

#ifdef LLVMJIT_DEBUG
	Value(F &cc, T val, const char *name="", const char *file=__builtin_FILE(), int line=__builtin_LINE()) : Value(cc, name, false, file, line) {
		*this = D2<T>{val, file, line};
	}
#else
	Value(F &cc, T val, const char *name="") : Value(cc, name) {
		*this = val;
	}
#endif
	// real copy requires new stack memory and copy of content
	Value(const Value &other) : Value(other.cc) {
		*this = other;
	}
	// move, just take the stack memory
	Value(const Value &&other) : ValueBase(std::move(other)) {}
	// move assign, just take stack memory location, copy of wrapper object
	Value &operator=(const Value &&other){ memreg = other.memreg; return *this; }

	// copy ctor for ref
	Value(const Ref<F,Value> &other) : Value(other.cc) {
		*this = other;
	}

	// explicit type conversion, assignment
	// always makes a copy
	// FIXME: implicit conversion between signed and unsigned, but not between types of same size ...
	template<class O>
	Value &narrow(const O &other){
		static_assert(sizeof(T) < sizeof(typename O::value_type), "narrowing conversion called on wrong types");
		store( cc.ir.CreateTrunc(other.load(), type()) );
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
			store( cc.ir.CreateSExt(other.load(), type()) );
		}else{
			store( cc.ir.CreateZExt(other.load(), type()) );
		}
		return *this;
	}

	// explicit conversion, no assignment, new value/temporary
	//TODO: decide on one style and delete other
	template<typename O>
	Value<F,O> narrow(){
		static_assert(sizeof(O) < sizeof(T), "narrowing conversion called on wrong types");
		Value<F,O> tmp(cc.ir, "narrowtmp");
		tmp.store( cc.ir.CreateTrunc(load(), tmp.type()) );
		return tmp;
	}
	template<typename O>
	Value<F,O> widen(){
		static_assert(sizeof(O) > sizeof(T), "widening conversion called on wrong types");
		Value<F,O> tmp(cc.ir, "widentmp");
		if constexpr(std::is_signed_v<T>){
			tmp.store( cc.ir.CreateSExt(load(), tmp.type()) );
		}else{
			tmp.store( cc.ir.CreateZExt(load(), tmp.type()) );
		}
		return tmp;
	}

	// assignment
	Value &operator=(const Value &other){ store( other.load() ); return *this; }
	Value &operator=(D2<value_type> other){
		DL2;
		store( llvm::ConstantInt::get(type(), OP2) );
		return *this;
	}
	Value &operator=(const Ref<F,Value> &other){ store( other.load() ); return *this; }
	//FIXME: takes any type
	Value &operator=(llvm::Value *val){ store( val ); return *this; }

	// special handling of bit tests, for convenience and performance
	void bit_test(const Value &bit, Label<F> &label, bool jump_on_set=true) const {
		Value testbit(cc.ir, value_type(1), "testbit");
		testbit <<= bit % 64;
		testbit &= *this; // test bit
		if(jump_on_set){
			jump(cc.ir, testbit != 0, label);
		}else{
			jump(cc.ir, testbit == 0, label);
		}
	}

	void bit_test_and_set(const Value &bit, Label<F> &label, bool jump_on_set=true){
		Value testbit(cc.ir, value_type(1), "testbit");
		testbit <<= bit % 64;
		auto cond = testbit & *this; // test bit
		operator|=(testbit); // set bit
		if(jump_on_set){
			jump(cc.ir, cond != 0, label);
		}else{
			jump(cc.ir, cond == 0, label);
		}
	}

	void bit_test_and_reset(const Value &bit, Label<F> &label, bool jump_on_set=true){
		Value testbit(cc, value_type(1), "testbit");
		testbit <<= bit % 64;
		auto cond = testbit & *this; // test bit
		operator&=(~testbit); // reset bit
		if(jump_on_set){
			jump(cc.ir, cond != 0, label);
		}else{
			jump(cc.ir, cond == 0, label);
		}
	}

	Value popcount() const {
		Value ret(cc);
		// call intrinsic
		llvm::CallInst *pop = cc.ir.CreateIntrinsic(llvm::Intrinsic::ctpop, {type()}, {load()});
		pop->setTailCall();
		ret.store(pop);
		return ret;
	}

	Value &operator<<=(const Value &other){ store( cc.ir.CreateShl(load(), other.load()) ); return *this; }
	Value &operator<<=(int amount){ store( cc.ir.CreateShl(load(), amount) ); return *this; }
	// memory operand not possible on right side

	Value &operator>>=(const Value &other){
		if constexpr(std::is_signed_v<T>){
			store( cc.ir.CreateAShr(load(), other.load()) );
		}else{
			store( cc.ir.CreateLShr(load(), other.load()) );
		}
		return *this;
	}
	Value &operator>>=(int amount){
		if constexpr(std::is_signed_v<T>){
			store( cc.ir.CreateAShr(load(), amount) );
		}else{
			store( cc.ir.CreateLShr(load(), amount) );
		}
		return *this;
	}
	// memory operand not possible on right side

	Value &operator*=(const Value &other){
		if constexpr(std::is_signed_v<T>){
			store( cc.ir.CreateNSWMul(load(), other.load()) );
		}else{
			store( cc.ir.CreateMul(load(), other.load()) );
		}
		return *this;
	}
	Value &operator*=(int constant){
		if constexpr(std::is_signed_v<T>){
			store( cc.ir.CreateNSWMul(load(), llvm::ConstantInt::get(type(), constant)) );
		}else{
			store( cc.ir.CreateMul(load(), llvm::ConstantInt::get(type(), constant)) );
		}
		return *this;
	}

	Value &operator/=(const Value &other){
		if constexpr(std::is_signed_v<T>){
			store( cc.ir.CreateSDiv(load(), other.load()) );
		}else{
			store( cc.ir.CreateUDiv(load(), other.load()) );
		}
		return *this;
	}
	Value &operator/=(int constant){
		if constexpr(std::is_signed_v<T>){
			store( cc.ir.CreateSDiv(load(), llvm::ConstantInt::get(type(), constant)) );
		}else{
			store( cc.ir.CreateUDiv(load(), llvm::ConstantInt::get(type(), constant)) );
		}
		return *this;
	}
	
	Value &operator%=(const Value &other){
		if constexpr(std::is_signed_v<T>){
			store( cc.ir.CreateSRem(load(), other.load()) );
		}else{
			store( cc.ir.CreateURem(load(), other.load()) );
		}
		return *this;
	}
	Value &operator%=(int constant){
		if constexpr(std::is_signed_v<T>){
			store( cc.ir.CreateSRem(load(), llvm::ConstantInt::get(type(), constant)) );
		}else{
			store( cc.ir.CreateURem(load(), llvm::ConstantInt::get(type(), constant)) );
		}
		return *this;
	}

	Value &operator+=(const D2<Value> &other){
		DL2;
		store( cc.ir.CreateAdd(load(), OP2.load()) );
		return *this;
	}
	Value &operator+=(int constant){ store( cc.ir.CreateAdd(load(), llvm::ConstantInt::get(type(),constant)) ); return *this; }
	Value &operator+=(const D2<Ref<F,Value>> &other){
		DL2;
		store( cc.ir.CreateAdd(load(), OP2.load()) );
		return *this;
	}

	Value &operator-=(const Value &other){ store( cc.ir.CreateSub(load(), other.load()) ); return *this; }
	Value &operator-=(int constant){ store( cc.ir.CreateSub(load(), llvm::ConstantInt::get(type(),constant)) ); return *this; }
	Value &operator-=(const Ref<F,Value> &other){ store( cc.ir.CreateSub(load(), other.load()) ); return *this; }

	Value &operator&=(const Value &other){        store( cc.ir.CreateAnd(load(), other.load()) ); return *this; }
	Value &operator&=(int constant){              store( cc.ir.CreateAnd(load(), constant) ); return *this; }
	Value &operator&=(const Ref<F,Value> &other){ store( cc.ir.CreateAnd(load(), other.load()) ); return *this; }

	Value &operator|=(const Value &other){        store( cc.ir.CreateOr(load(), other.load()) ); return *this; }
	Value &operator|=(int constant){              store( cc.ir.CreateOr(load(), constant) ); return *this; }
	Value &operator|=(const Ref<F,Value> &other){ store( cc.ir.CreateOr(load(), other.load()) ); return *this; }

	Value &operator^=(const Value &other){        store( cc.ir.CreateXor(load(), other.load()) ); return *this; }
	Value &operator^=(int constant){              store( cc.ir.CreateXor(load(), constant) ); return *this; }
	Value &operator^=(const Ref<F,Value> &other){ store( cc.ir.CreateXor(load(), other.load()) ); return *this; }

	Value &operator~(){ store( cc.ir.CreateNot(load()) ); return *this; }

	// operators creatting temporary
	LLVMJIT_OPERATORS_WITH_TEMPORARIES(Value)

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
template<typename T> Value(LLVMBuilders&, T val, const char *) -> Value<LLVMBuilders,T>;
template<typename FnPtr, typename T> Value(Function<runtimellvmjit,FnPtr>&, T val, const char *) -> Value<LLVMBuilders,T>;
template<typename T> Value(const Ref<LLVMBuilders,Value<LLVMBuilders,T>>&) -> Value<LLVMBuilders,T>;


template<typename T>
Value<LLVMBuilders,T> make_value(LLVMBuilders &cc, Condition<LLVMBuilders> &&cond){
	Value<LLVMBuilders,T> val(cc);
	cond.compare();
	val.store( cc.ir.CreateZExt(cond.cmp_result, val.type()) );
	return val;
}


} // namespace

#endif
