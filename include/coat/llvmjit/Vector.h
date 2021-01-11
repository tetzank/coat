#ifndef COAT_LLVMJIT_VECTOR_H_
#define COAT_LLVMJIT_VECTOR_H_

#include "Ptr.h"
#include "VectorMask.h"


namespace coat {

template<typename T, unsigned width>
struct Vector<LLVMBuilders,T,width> final {
	using F = LLVMBuilders;
	using value_type = T;

	static_assert(sizeof(T)==1 || sizeof(T)==2 || sizeof(T)==4 || sizeof(T)==8,
		"only plain arithmetic types supported of sizes: 1, 2, 4 or 8 bytes");
	static_assert(std::is_signed_v<T> || std::is_unsigned_v<T>,
		"only plain signed or unsigned arithmetic types supported");
	static_assert(sizeof(T)*width == 128/8 || sizeof(T)*width == 256/8,
		"only 128-bit and 256-bit vector instructions supported at the moment");

	F &cc;
	llvm::Value *memreg;

	llvm::Value *load() const { return cc.ir.CreateLoad(memreg, "load"); }
	void store(llvm::Value *v) { cc.ir.CreateStore(v, memreg); }
	llvm::Type *type() const { return ((llvm::PointerType*)memreg->getType())->getElementType(); }

	llvm::Value *refload(const Ref<F,Value<F,T>> &other) const {
		// cast array ptr to vector ptr
		llvm::Value *vecptr = cc.ir.CreateBitCast(other.mem, memreg->getType());
		return cc.ir.CreateLoad(vecptr, "refload");
	}

	operator const llvm::Value*() const { return load(); }
	operator       llvm::Value*()       { return load(); }

	unsigned getWidth() const { return width; }

	Vector(F &cc, const char *name="") : cc(cc) {
		llvm::Type *element_type;
		// llvm IR has no types for unsigned/signed integers
		switch(sizeof(T)){
			case 1: element_type = llvm::Type::getInt8Ty (cc.ir.getContext()); break;
			case 2: element_type = llvm::Type::getInt16Ty(cc.ir.getContext()); break;
			case 4: element_type = llvm::Type::getInt32Ty(cc.ir.getContext()); break;
			case 8: element_type = llvm::Type::getInt64Ty(cc.ir.getContext()); break;
		}
		memreg = allocateStackVariable(cc.ir, llvm::FixedVectorType::get(element_type, width), name);
	}

	// copy ctor
	Vector(const Vector &other) = delete; //FIXME: no real copy for now
	// move ctor, just "steal" memreg
	Vector(Vector &&other) : cc(other.cc), memreg(other.memreg) {}

	// copy assignment with real copy
	Vector &operator=(const Vector &other) = delete; //FIXME: no real copy for now
	// copy assignment from temporary, "steal" memory location
	Vector &operator=(Vector &&other){
		memreg = other.memreg;
		return *this;
	}

	//TODO: aligned load & store
	Vector &operator=(Ref<F,Value<F,T>> &&src){ store( refload(src) ); return *this; }

	//FIXME: reference as parameter looks unintuitive, pointer would make more sense from a C++ perspective
	void store(Ref<F,Value<F,T>> &&dest) const {
		// cast array ptr to vector ptr
		llvm::Value *vecptr = cc.ir.CreateBitCast(dest.mem, memreg->getType());
		cc.ir.CreateStore(load(), vecptr);
	}

	//TODO: storing to reference is unintuitive, change to pointer
	void compressstore(Ref<F,Value<F,T>> &&dest, const VectorMask<F,width> &mask) const {
		// call intrinsic
		// fallback to generic code when no AVX512
		//TODO: the generic fallback is not efficient, roll our own
		llvm::CallInst *pop = cc.ir.CreateIntrinsic(
			llvm::Intrinsic::masked_compressstore,
			type(),
			{load(), dest.mem, mask.load()}
		);
		pop->setTailCall();
	}

	// vector types are first class in LLVM IR, most operations accept them as operands
	Vector &operator+=(const Vector &other){            store( cc.ir.CreateAdd(load(), other.load())   ); return *this; }
	Vector &operator+=(const Ref<F,Value<F,T>> &other){ store( cc.ir.CreateAdd(load(), refload(other)) ); return *this; }

	Vector &operator-=(const Vector &other){            store( cc.ir.CreateSub(load(), other.load())   ); return *this; }
	Vector &operator-=(const Ref<F,Value<F,T>> &other){ store( cc.ir.CreateSub(load(), refload(other)) ); return *this; }

	Vector &operator/=(int amount){
		llvm::VectorType *vt = (llvm::VectorType*)type();
		llvm::Constant *splat = llvm::ConstantVector::getSplat(
			vt->getElementCount(),
			llvm::ConstantInt::get( vt->getElementType(), amount )
		);
		if constexpr(std::is_signed_v<T>){
			store( cc.ir.CreateSDiv(load(), splat) );
		}else{
			store( cc.ir.CreateUDiv(load(), splat) );
		}
		return *this;
	}


	Vector &operator<<=(int amount){          store( cc.ir.CreateShl(load(), amount) ); return *this; }
	Vector &operator<<=(const Vector &other){ store( cc.ir.CreateShl(load(), other)  ); return *this; }

	Vector &operator>>=(int amount){
		if constexpr(std::is_signed_v<T>){
			store( cc.ir.CreateAShr(load(), amount) );
		}else{
			store( cc.ir.CreateLShr(load(), amount) );
		}
		return *this;
	}
	Vector &operator>>=(const Vector &other){
		if constexpr(std::is_signed_v<T>){
			store( cc.ir.CreateAShr(load(), other) );
		}else{
			store( cc.ir.CreateLShr(load(), other) );
		}
		return *this;
	}

	Vector &operator&=(const Vector &other){ store( cc.ir.CreateAnd(load(), other.load()) ); return *this; }
	Vector &operator|=(const Vector &other){ store( cc.ir.CreateOr (load(), other.load()) ); return *this; }
	Vector &operator^=(const Vector &other){ store( cc.ir.CreateXor(load(), other.load()) ); return *this; }


	// comparisons
	VectorMask<F,width> operator==(const Vector &other) const {
		VectorMask<F,width> mask(cc);
		mask.store( cc.ir.CreateICmpEQ(load(), other.load()) );
		return mask;
	}
	VectorMask<F,width> operator!=(const Vector &other) const {
		VectorMask<F,width> mask(cc);
		mask.store( cc.ir.CreateICmpNE(load(), other.load()) );
		return mask;
	}
	//TODO: define all other comparisons
};


template<int width, typename T>
Vector<LLVMBuilders,T,width> make_vector(LLVMBuilders &cc, Ref<LLVMBuilders,Value<LLVMBuilders,T>> &&src){
	Vector<LLVMBuilders,T,width> v(cc);
	v = std::move(src);
	return v;
}

template<typename T, unsigned width>
Vector<LLVMBuilders,T,width> shuffle(
	LLVMBuilders &cc,
	const Vector<LLVMBuilders,T,width> &vec,
	std::array<int,width> &&indexes
){
	Vector<LLVMBuilders,T,width> res(cc);
	llvm::Value *shuffled = cc.ir.CreateShuffleVector(vec.load(), llvm::UndefValue::get(vec.type()), indexes);
	res.store(shuffled);
	return res;
}

template<typename T, unsigned width>
Vector<LLVMBuilders,T,width> shuffle(
	LLVMBuilders &cc,
	const Vector<LLVMBuilders,T,width> &vec0,
	const Vector<LLVMBuilders,T,width> &vec1,
	std::array<unsigned,width> &&indexes
){
	Vector<LLVMBuilders,T,width> res(cc);
	llvm::Value *shuffled = cc.ir.CreateShuffleVector(vec0, vec1, indexes);
	res.store(shuffled);
	return res;
}

template<typename T, unsigned width>
Vector<LLVMBuilders,T,width> min(
	LLVMBuilders &cc,
	const Vector<LLVMBuilders,T,width> &vec0,
	const Vector<LLVMBuilders,T,width> &vec1
){
	Vector<LLVMBuilders,T,width> res(cc);
	llvm::Value *v0 = vec0.load(), *v1 = vec1.load();
	llvm::Value *comparison;
	if constexpr(std::is_signed_v<T>){
		comparison = cc.ir.CreateICmpSLT(v0, v1);
	}else{
		comparison = cc.ir.CreateICmpULT(v0, v1);
	}
	llvm::Value *s = cc.ir.CreateSelect(comparison, v0, v1);
	res.store(s);
	return res;
}

template<typename T, unsigned width>
Vector<LLVMBuilders,T,width> max(
	LLVMBuilders &cc,
	const Vector<LLVMBuilders,T,width> &vec0,
	const Vector<LLVMBuilders,T,width> &vec1
){
	Vector<LLVMBuilders,T,width> res(cc);
	llvm::Value *v0 = vec0.load(), *v1 = vec1.load();
	llvm::Value *comparison;
	if constexpr(std::is_signed_v<T>){
		comparison = cc.ir.CreateICmpSGT(v0, v1);
	}else{
		comparison = cc.ir.CreateICmpUGT(v0, v1);
	}
	llvm::Value *s = cc.ir.CreateSelect(comparison, v0, v1);
	res.store(s);
	return res;
}


} // namespace

#endif
