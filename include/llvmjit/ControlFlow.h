#ifndef COAT_LLVMJIT_CONTROLFLOW_H_
#define COAT_LLVMJIT_CONTROLFLOW_H_


namespace coat {

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
void if_then(llvm::IRBuilder<> &builder, Condition<::llvm::IRBuilder<>> cond, Fn &&then){
	llvm::BasicBlock *bb_current = builder.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_then = llvm::BasicBlock::Create(builder.getContext(), "then", fn);
	llvm::BasicBlock *bb_afterthen = llvm::BasicBlock::Create(builder.getContext(), "afterthen", fn);
	// check
	jump(builder, cond, bb_then, bb_afterthen); // if not jump over

	builder.SetInsertPoint(bb_then);
	then();
	// if then() already emitted a terminator (e.g., early exit with ret), don't emit branch
	// sometimes there is a cleanupret, which is a terminator but should be ignored
	//if(!builder.GetInsertBlock()->back().isTerminator()){
	// just check for the return instruction as we actually just care about an early exit for now
	if(builder.GetInsertBlock()->back().getOpcode() != llvm::Instruction::Ret){
		jump(builder, bb_afterthen);
#if 0
	}else{
		//DEBUG
		llvm::BasicBlock *bb = builder.GetInsertBlock();
		printf("found terminator in BB %s: %s (%u)\n", bb->getName().data(), bb->back().getOpcodeName(), bb->back().getOpcode());
#endif
	}

	// label after then branch
	builder.SetInsertPoint(bb_afterthen);
}

template<typename Then, typename Else>
void if_then_else(llvm::IRBuilder<> &builder, Condition<::llvm::IRBuilder<>> cond, Then &&then, Else &&else_){
	llvm::BasicBlock *bb_current = builder.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_then = llvm::BasicBlock::Create(builder.getContext(), "then", fn);
	llvm::BasicBlock *bb_else = llvm::BasicBlock::Create(builder.getContext(), "else", fn);
	llvm::BasicBlock *bb_after = llvm::BasicBlock::Create(builder.getContext(), "after", fn);
	// check
	jump(builder, cond, bb_then, bb_else); // if not jump to else

	builder.SetInsertPoint(bb_then);
	then();
	// if then() already emitted a ret, don't emit branch
	// see if_then() for details
	if(builder.GetInsertBlock()->back().getOpcode() != llvm::Instruction::Ret){
		jump(builder, bb_after);
	}

	builder.SetInsertPoint(bb_else);
	else_();
	// if else_() already emitted a ret, don't emit branch
	if(builder.GetInsertBlock()->back().getOpcode() != llvm::Instruction::Ret){
		jump(builder, bb_after);
	}

	// label after then branch
	builder.SetInsertPoint(bb_after);
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

template<typename Fn>
void do_while(llvm::IRBuilder<> &builder, Fn &&body, Condition<::llvm::IRBuilder<>> cond){
	llvm::BasicBlock *bb_current = builder.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(builder.getContext(), "loop", fn);
	llvm::BasicBlock *bb_afterloop = llvm::BasicBlock::Create(builder.getContext(), "afterloop", fn);

	// no checking, but still have to jump to basic block containing the loop
	jump(builder, bb_loop);

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


} // namespace

#endif
