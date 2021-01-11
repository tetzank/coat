#ifndef COAT_LLVMJIT_VALUEBASE_H_
#define COAT_LLVMJIT_VALUEBASE_H_

#include "Function.h"


namespace coat {

template<>
struct ValueBase<LLVMBuilders> {
	LLVMBuilders &cc;
	llvm::Value *memreg;

	ValueBase(LLVMBuilders &cc) : cc(cc) {}
	// move, just take stack memory
	//ValueBase(const ValueBase &&other) : cc(other.cc), memreg(other.memreg) {}

	llvm::Value *load() const { return cc.ir.CreateLoad(memreg, "load"); }
	void store(llvm::Value *v) { cc.ir.CreateStore(v, memreg); }
	llvm::Type *type() const { return ((llvm::PointerType*)memreg->getType())->getElementType(); }

	operator const llvm::Value*() const { return load(); }
	operator       llvm::Value*()       { return load(); }

	// identical operations for signed and unsigned, or different sizes
	// pre-increment, post-increment not supported as it leads to temporary
	ValueBase &operator++(){ store( cc.ir.CreateAdd(load(), llvm::ConstantInt::get(type(), 1), "inc") ); return *this; }
	// pre-decrement
	ValueBase &operator--(){ store( cc.ir.CreateSub(load(), llvm::ConstantInt::get(type(), 1), "dec") ); return *this; }
};

} // namespace

#endif
