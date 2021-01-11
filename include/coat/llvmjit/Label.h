#ifndef COAT_LLVMJIT_LABEL_H_
#define COAT_LLVMJIT_LABEL_H_


namespace coat {

template<>
struct Label<LLVMBuilders> final {
	using F = LLVMBuilders;

	LLVMBuilders &cc;
	llvm::BasicBlock *bb_label;

	Label(LLVMBuilders &cc) : cc(cc) {
		bb_label = llvm::BasicBlock::Create(cc.ir.getContext(), "label", cc.ir.GetInsertBlock()->getParent());
	}

	void bind() {
		// implicit fallthrough, branch from current bb to label
		cc.ir.CreateBr(bb_label);

		cc.ir.SetInsertPoint(bb_label);
	}

	operator const llvm::BasicBlock*() const { return bb_label; }
	operator       llvm::BasicBlock*()       { return bb_label; }
};

// deduction guides
template<typename FnPtr> Label(Function<runtimellvmjit,FnPtr>&) -> Label<LLVMBuilders>;

} // namespace

#endif
