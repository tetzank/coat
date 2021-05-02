#ifndef COAT_LLVMJIT_REF_H_
#define COAT_LLVMJIT_REF_H_

#include "operator_helper.h"


namespace coat {

template<class T>
struct Ref<LLVMBuilders,T> {
	using F = LLVMBuilders;
	using inner_type = T;

	LLVMBuilders &cc;
	llvm::Value *mem;

	llvm::Value *load() const { return cc.ir.CreateLoad(mem, "memload"); }
	//void store(llvm::Value *v) { cc.ir.CreateStore(v, mem); }
	llvm::Type *type() const { return ((llvm::PointerType*)mem->getType())->getElementType(); }


	Ref(LLVMBuilders &cc, llvm::Value *mem) : cc(cc), mem(mem) {}

	Ref &operator=(const T &other){
		cc.ir.CreateStore(other.load(), mem);
		return *this;
	}
	Ref &operator=(typename inner_type::value_type value){
		cc.ir.CreateStore(llvm::ConstantInt::get(type(), value), mem);
		return *this;
	}

	// operators creating temporary virtual registers
	LLVMJIT_OPERATORS_WITH_TEMPORARIES(T)

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
