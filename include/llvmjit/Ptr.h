#ifndef LLVMJIT_PTR_H_
#define LLVMJIT_PTR_H_

#include "ValueBase.h"
//#include "Ref.h"


template<class T>
struct Ptr<::llvm::IRBuilder<>,T> {
	using F = ::llvm::IRBuilder<>;
	using value_type = typename T::value_type;
	using value_base_type = ValueBase<F>;
	using mem_type = Ref<F,T>;

	static_assert(std::is_base_of_v<value_base_type,T>, "pointer type only of register wrappers");

	llvm::IRBuilder<> &builder;
	llvm::Value *memreg;

	llvm::Value *load() const { return builder.CreateLoad(memreg, "load"); }
	void store(llvm::Value *v) { builder.CreateStore(v, memreg); }
	llvm::Type *type() const { return ((llvm::PointerType*)memreg->getType())->getElementType(); }

	Ptr(llvm::IRBuilder<> &builder, const char *name="") : builder(builder) {
		// llvm IR has no types for unsigned/signed integers
		switch(sizeof(value_type)){
			case 1: memreg = builder.CreateAlloca(llvm::Type::getInt8PtrTy (builder.getContext()), nullptr, name); break;
			case 2: memreg = builder.CreateAlloca(llvm::Type::getInt16PtrTy(builder.getContext()), nullptr, name); break;
			case 4: memreg = builder.CreateAlloca(llvm::Type::getInt32PtrTy(builder.getContext()), nullptr, name); break;
			case 8: memreg = builder.CreateAlloca(llvm::Type::getInt64PtrTy(builder.getContext()), nullptr, name); break;
		}
	}
	//FIXME: takes any type
	Ptr &operator=(llvm::Value *val){ store( val ); return *this; }

	// dereference
	mem_type operator*(){
		return { builder, builder.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), 0)) };
	}
	// indexing with variable
	mem_type operator[](const value_base_type &idx){
		return { builder, builder.CreateGEP(load(), idx.load()) };
	}
	// indexing with constant -> use offset
	mem_type operator[](size_t idx){
		return { builder, builder.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), idx)) };
	}
	
	Ptr operator+(const value_base_type &value) const {
		Ptr res(builder);
		res.store( builder.CreateGEP(load(), value.load()) );
		return res;
	}
	Ptr operator+(size_t value) const {
		Ptr res(builder);
		res.store( builder.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), value)) );
		return res;
	}

	Ptr &operator+=(const value_base_type &value){
		store( builder.CreateGEP(load(), value.load()) );
		return *this;
	}
	Ptr &operator+=(int amount){
		store( builder.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), amount)) );
		return *this;
	}
	Ptr &operator-=(int amount){
		store( builder.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), -amount)) );
		return *this;
	}

	// pre-increment, post-increment not provided as it creates temporary
	Ptr &operator++(){
		store( builder.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), 1)) );
		return *this;
	}
	// pre-decrement
	Ptr &operator--(){
		store( builder.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), -1)) );
		return *this;
	}

	// comparisons
	Condition<F> operator==(const Ptr &other) const { return {builder, memreg, other.memreg, ConditionFlag::e};  }
	Condition<F> operator!=(const Ptr &other) const { return {builder, memreg, other.memreg, ConditionFlag::ne}; }
};


#endif
