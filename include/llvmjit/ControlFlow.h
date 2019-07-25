#ifndef LLVMJIT_CONTROLFLOW_H_
#define LLVMJIT_CONTROLFLOW_H_


inline void jump(llvm::IRBuilder<> &builder, llvm::BasicBlock *bb_dest){
	builder.CreateBr(bb_dest);
}
inline void jump(llvm::IRBuilder<> &builder, Condition<::llvm::IRBuilder<>> cond, llvm::BasicBlock *bb_success, llvm::BasicBlock *bb_fail){
	cond.compare();
	cond.jump(bb_success, bb_fail);
}


inline void ret(::llvm::IRBuilder<> &builder){
	builder.CreateRetVoid();
}
template<typename FnPtr, typename VReg>
inline void ret(Function<runtimellvmjit,FnPtr> &ctx, VReg &reg){
	static_assert(std::is_same_v<typename Function<runtimellvmjit,FnPtr>::return_type, typename VReg::value_type>, "incompatible return types");
	ctx.builder.CreateRet(reg.load());
}


template<typename Fn>
void loop_while(llvm::IRBuilder<> &builder, Condition<::llvm::IRBuilder<>> cond, Fn &&body){
	llvm::BasicBlock *bb_current = builder.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(builder.getContext(), "loop", fn);
	llvm::BasicBlock *bb_afterloop = llvm::BasicBlock::Create(builder.getContext(), "afterloop", fn);

	// check if even one iteration
	jump(builder, cond, bb_loop, bb_afterloop); // if not jump over

	// loop
	builder.SetInsertPoint(bb_loop);
		body();
	jump(builder, cond, bb_loop, bb_afterloop);

	// label after loop body
	builder.SetInsertPoint(bb_afterloop);
}

template<class Ptr, typename Fn>
void for_each(llvm::IRBuilder<> &builder, Ptr &begin, const Ptr &end, Fn &&body){
	llvm::BasicBlock *bb_current = builder.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(builder.getContext(), "foreach", fn);
	llvm::BasicBlock *bb_after = llvm::BasicBlock::Create(builder.getContext(), "after", fn);

	// check if even one iteration
	jump(builder, begin == end, bb_after, bb_loop);

	// loop over all elements
	builder.SetInsertPoint(bb_loop);
		typename Ptr::mem_type vr_ele = *begin;
		body(vr_ele);
		++begin;
	jump(builder, begin != end, bb_loop, bb_after);

	// label after loop body
	builder.SetInsertPoint(bb_after);
}


#endif
