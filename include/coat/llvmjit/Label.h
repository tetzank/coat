#ifndef COAT_LLVMJIT_LABEL_H_
#define COAT_LLVMJIT_LABEL_H_


namespace coat {

template<>
struct Label<llvm::IRBuilder<>> final {
	using F = ::llvm::IRBuilder<>;

	llvm::IRBuilder<> &cc;
	llvm::BasicBlock *bb_label;

	Label(llvm::IRBuilder<> &cc) : cc(cc) {
		bb_label = llvm::BasicBlock::Create(cc.getContext(), "label", cc.GetInsertBlock()->getParent());
	}

	void bind() {
		// implicit fallthrough, branch from current bb to label
		cc.CreateBr(bb_label);

		cc.SetInsertPoint(bb_label);
	}

	operator const llvm::BasicBlock*() const { return bb_label; }
	operator       llvm::BasicBlock*()       { return bb_label; }
};

// deduction guides
template<typename FnPtr> Label(Function<runtimellvmjit,FnPtr>&) -> Label<llvm::IRBuilder<>>;

} // namespace

#endif
