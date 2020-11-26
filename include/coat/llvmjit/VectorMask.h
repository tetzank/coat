#ifndef COAT_LLVMJIT_VECTORMASK_H_
#define COAT_LLVMJIT_VECTORMASK_H_


namespace coat {

template<unsigned width>
struct VectorMask<::llvm::IRBuilder<>,width> final {
	using F = ::llvm::IRBuilder<>;

	F &cc;
	llvm::Value *memreg;

	llvm::Value *load() const { return cc.CreateLoad(memreg, "load"); }
	void store(llvm::Value *v) { cc.CreateStore(v, memreg); }
	llvm::Type *type() const { return ((llvm::PointerType*)memreg->getType())->getElementType(); }

	unsigned getWidth() const { return width; }

	VectorMask(F &cc, const char *name="") : cc(cc) {
		memreg = allocateStackVariable(cc, llvm::FixedVectorType::get(llvm::Type::getInt1Ty(cc.getContext()), width), name);
	}

	Value<F,uint64_t> movemask() const {
		Value<F,uint64_t> result(cc);
		llvm::VectorType *vt = (llvm::VectorType*)type();
		llvm::Constant *zeroinitializer = llvm::ConstantVector::getSplat(
			vt->getElementCount(),
			llvm::ConstantInt::get( vt->getElementType(), 0 )
		);
		llvm::Value *cmp = cc.CreateICmpSLT(load(), zeroinitializer);
		llvm::Value *cast = cc.CreateBitCast(cmp, llvm::IntegerType::get(cc.getContext(), width));
		result.store( cc.CreateZExt(cast, result.type()) );
		return result;
	}

	VectorMask &operator&=(const VectorMask &other){ store( cc.CreateAnd(load(), other.load()) ); return *this; }
	VectorMask &operator|=(const VectorMask &other){ store( cc.CreateOr (load(), other.load()) ); return *this; }
	VectorMask &operator^=(const VectorMask &other){ store( cc.CreateXor(load(), other.load()) ); return *this; }
};

} // namespace

#endif
