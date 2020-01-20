#ifndef COAT_LLVMJIT_VECTOR_H_
#define COAT_LLVMJIT_VECTOR_H_

#include "Ptr.h"


namespace coat {

template<typename T, unsigned width>
struct Vector<::llvm::IRBuilder<>,T,width> final {
	using F = ::llvm::IRBuilder<>;
	using value_type = T;

	static_assert(sizeof(T)==1 || sizeof(T)==2 || sizeof(T)==4 || sizeof(T)==8,
		"only plain arithmetic types supported of sizes: 1, 2, 4 or 8 bytes");
	static_assert(std::is_signed_v<T> || std::is_unsigned_v<T>,
		"only plain signed or unsigned arithmetic types supported");
	static_assert(sizeof(T)*width == 128/8 || sizeof(T)*width == 256/8,
		"only 128-bit and 256-bit vector instructions supported at the moment");

	F &cc;
	llvm::Value *memreg;

	llvm::Value *load() const { return cc.CreateLoad(memreg, "load"); }
	void store(llvm::Value *v) { cc.CreateStore(v, memreg); }
	llvm::Type *type() const { return ((llvm::PointerType*)memreg->getType())->getElementType(); }

	llvm::Value *refload(const Ref<F,Value<F,T>> &other) const {
		// cast array ptr to vector ptr
		llvm::Value *vecptr = cc.CreateBitCast(other.mem, memreg->getType());
		return cc.CreateLoad(vecptr, "refload");
	}

	operator const llvm::Value*() const { return load(); }
	operator       llvm::Value*()       { return load(); }

	Vector(F &cc, const char *name="") : cc(cc) {
		llvm::Type *element_type;
		// llvm IR has no types for unsigned/signed integers
		switch(sizeof(T)){
			case 1: element_type = llvm::Type::getInt8Ty (cc.getContext()); break;
			case 2: element_type = llvm::Type::getInt16Ty(cc.getContext()); break;
			case 4: element_type = llvm::Type::getInt32Ty(cc.getContext()); break;
			case 8: element_type = llvm::Type::getInt64Ty(cc.getContext()); break;
		}
		memreg = allocateStackVariable(cc, llvm::VectorType::get(element_type, width), name);
	}

	unsigned getWidth() const { return width; }

	//TODO: aligned load & store
	Vector &operator=(Ref<F,Value<F,T>> &&src){ store( refload(src) ); return *this; }

	void store(Ref<F,Value<F,T>> &&dest) const {
		// cast array ptr to vector ptr
		llvm::Value *vecptr = cc.CreateBitCast(dest.mem, memreg->getType());
		cc.CreateStore(load(), vecptr);
	}


	// vector types are first class in LLVM IR, most operations accept them as operands
	Vector &operator+=(const Vector &other){            store( cc.CreateAdd(load(), other.load())   ); return *this; }
	Vector &operator+=(const Ref<F,Value<F,T>> &other){ store( cc.CreateAdd(load(), refload(other)) ); return *this; }

	Vector &operator-=(const Vector &other){            store( cc.CreateSub(load(), other.load())   ); return *this; }
	Vector &operator-=(const Ref<F,Value<F,T>> &other){ store( cc.CreateSub(load(), refload(other)) ); return *this; }

	Vector &operator/=(int amount){
		llvm::Constant *splat = llvm::ConstantVector::getSplat(
			width,
			llvm::ConstantInt::get( ((llvm::VectorType*)type())->getElementType(), amount )
		);
		if constexpr(std::is_signed_v<T>){
			store( cc.CreateSDiv(load(), splat) );
		}else{
			store( cc.CreateUDiv(load(), splat) );
		}
		return *this;
	}


	Vector &operator<<=(int amount){ store( cc.CreateShl(load(), amount) ); return *this; }

	Vector &operator>>=(int amount){
		if constexpr(std::is_signed_v<T>){
			store( cc.CreateAShr(load(), amount) );
		}else{
			store( cc.CreateLShr(load(), amount) );
		}
		return *this;
	}

	Vector &operator&=(const Vector &other){ store( cc.CreateAnd(load(), other) ); return *this; }
	Vector &operator|=(const Vector &other){ store( cc.CreateOr (load(), other) ); return *this; }
	Vector &operator^=(const Vector &other){ store( cc.CreateXor(load(), other) ); return *this; }
};


template<int width, typename T>
Vector<llvm::IRBuilder<>,T,width> make_vector(llvm::IRBuilder<> &cc, Ref<llvm::IRBuilder<>,Value<llvm::IRBuilder<>,T>> &&src){
	Vector<llvm::IRBuilder<>,T,width> v(cc);
	v = std::move(src);
	return v;
}

} // namespace

#endif
