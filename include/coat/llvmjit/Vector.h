#ifndef COAT_LLVMJIT_VECTOR_H_
#define COAT_LLVMJIT_VECTOR_H_

#include "Ptr.h"
#include "VectorMask.h"


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

	unsigned getWidth() const { return width; }

	Vector(F &cc, const char *name="") : cc(cc) {
		llvm::Type *element_type;
		// llvm IR has no types for unsigned/signed integers
		switch(sizeof(T)){
			case 1: element_type = llvm::Type::getInt8Ty (cc.getContext()); break;
			case 2: element_type = llvm::Type::getInt16Ty(cc.getContext()); break;
			case 4: element_type = llvm::Type::getInt32Ty(cc.getContext()); break;
			case 8: element_type = llvm::Type::getInt64Ty(cc.getContext()); break;
		}
		memreg = allocateStackVariable(cc, llvm::FixedVectorType::get(element_type, width), name);
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
		llvm::Value *vecptr = cc.CreateBitCast(dest.mem, memreg->getType());
		cc.CreateStore(load(), vecptr);
	}

	//TODO: storing to reference is unintuitive, change to pointer
	void compressstore(Ref<F,Value<F,T>> &&dest, const VectorMask<F,width> &mask) const {
		// call intrinsic
		// fallback to generic code when no AVX512
		//TODO: the generic fallback is not efficient, roll our own
		llvm::CallInst *pop = cc.CreateIntrinsic(
			llvm::Intrinsic::masked_compressstore,
			type(),
			{load(), dest.mem, mask.load()}
		);
		pop->setTailCall();
	}

	// vector types are first class in LLVM IR, most operations accept them as operands
	Vector &operator+=(const Vector &other){            store( cc.CreateAdd(load(), other.load())   ); return *this; }
	Vector &operator+=(const Ref<F,Value<F,T>> &other){ store( cc.CreateAdd(load(), refload(other)) ); return *this; }

	Vector &operator-=(const Vector &other){            store( cc.CreateSub(load(), other.load())   ); return *this; }
	Vector &operator-=(const Ref<F,Value<F,T>> &other){ store( cc.CreateSub(load(), refload(other)) ); return *this; }

	Vector &operator/=(int amount){
		llvm::VectorType *vt = (llvm::VectorType*)type();
		llvm::Constant *splat = llvm::ConstantVector::getSplat(
			vt->getElementCount(),
			llvm::ConstantInt::get( vt->getElementType(), amount )
		);
		if constexpr(std::is_signed_v<T>){
			store( cc.CreateSDiv(load(), splat) );
		}else{
			store( cc.CreateUDiv(load(), splat) );
		}
		return *this;
	}


	Vector &operator<<=(int amount){          store( cc.CreateShl(load(), amount) ); return *this; }
	Vector &operator<<=(const Vector &other){ store( cc.CreateShl(load(), other)  ); return *this; }

	Vector &operator>>=(int amount){
		if constexpr(std::is_signed_v<T>){
			store( cc.CreateAShr(load(), amount) );
		}else{
			store( cc.CreateLShr(load(), amount) );
		}
		return *this;
	}
	Vector &operator>>=(const Vector &other){
		if constexpr(std::is_signed_v<T>){
			store( cc.CreateAShr(load(), other) );
		}else{
			store( cc.CreateLShr(load(), other) );
		}
		return *this;
	}

	Vector &operator&=(const Vector &other){ store( cc.CreateAnd(load(), other.load()) ); return *this; }
	Vector &operator|=(const Vector &other){ store( cc.CreateOr (load(), other.load()) ); return *this; }
	Vector &operator^=(const Vector &other){ store( cc.CreateXor(load(), other.load()) ); return *this; }


	// comparisons
	VectorMask<F,width> operator==(const Vector &other) const {
		VectorMask<F,width> mask(cc);
		mask.store( cc.CreateICmpEQ(load(), other.load()) );
		return mask;
	}
	VectorMask<F,width> operator!=(const Vector &other) const {
		VectorMask<F,width> mask(cc);
		mask.store( cc.CreateICmpNE(load(), other.load()) );
		return mask;
	}
	//TODO: define all other comparisons
};


template<int width, typename T>
Vector<llvm::IRBuilder<>,T,width> make_vector(llvm::IRBuilder<> &cc, Ref<llvm::IRBuilder<>,Value<llvm::IRBuilder<>,T>> &&src){
	Vector<llvm::IRBuilder<>,T,width> v(cc);
	v = std::move(src);
	return v;
}

template<typename T, unsigned width>
Vector<llvm::IRBuilder<>,T,width> shuffle(
	llvm::IRBuilder<> &cc,
	const Vector<llvm::IRBuilder<>,T,width> &vec,
	std::array<int,width> &&indexes
){
	Vector<llvm::IRBuilder<>,T,width> res(cc);
	llvm::Value *shuffled = cc.CreateShuffleVector(vec.load(), llvm::UndefValue::get(vec.type()), indexes);
	res.store(shuffled);
	return res;
}

template<typename T, unsigned width>
Vector<llvm::IRBuilder<>,T,width> shuffle(
	llvm::IRBuilder<> &cc,
	const Vector<llvm::IRBuilder<>,T,width> &vec0,
	const Vector<llvm::IRBuilder<>,T,width> &vec1,
	std::array<unsigned,width> &&indexes
){
	Vector<llvm::IRBuilder<>,T,width> res(cc);
	llvm::Value *shuffled = cc.CreateShuffleVector(vec0, vec1, indexes);
	res.store(shuffled);
	return res;
}

template<typename T, unsigned width>
Vector<llvm::IRBuilder<>,T,width> min(
	llvm::IRBuilder<> &cc,
	const Vector<llvm::IRBuilder<>,T,width> &vec0,
	const Vector<llvm::IRBuilder<>,T,width> &vec1
){
	Vector<llvm::IRBuilder<>,T,width> res(cc);
	llvm::Value *v0 = vec0.load(), *v1 = vec1.load();
	llvm::Value *comparison;
	if constexpr(std::is_signed_v<T>){
		comparison = cc.CreateICmpSLT(v0, v1);
	}else{
		comparison = cc.CreateICmpULT(v0, v1);
	}
	llvm::Value *s = cc.CreateSelect(comparison, v0, v1);
	res.store(s);
	return res;
}

template<typename T, unsigned width>
Vector<llvm::IRBuilder<>,T,width> max(
	llvm::IRBuilder<> &cc,
	const Vector<llvm::IRBuilder<>,T,width> &vec0,
	const Vector<llvm::IRBuilder<>,T,width> &vec1
){
	Vector<llvm::IRBuilder<>,T,width> res(cc);
	llvm::Value *v0 = vec0.load(), *v1 = vec1.load();
	llvm::Value *comparison;
	if constexpr(std::is_signed_v<T>){
		comparison = cc.CreateICmpSGT(v0, v1);
	}else{
		comparison = cc.CreateICmpUGT(v0, v1);
	}
	llvm::Value *s = cc.CreateSelect(comparison, v0, v1);
	res.store(s);
	return res;
}


} // namespace

#endif
