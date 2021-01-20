#ifndef COAT_LLVMJIT_CONTROLFLOW_H_
#define COAT_LLVMJIT_CONTROLFLOW_H_


namespace coat {

inline void jump(LLVMBuilders &cc, llvm::BasicBlock *bb_dest){
	cc.ir.CreateBr(bb_dest);
}
inline void jump(LLVMBuilders &, Condition<LLVMBuilders> cond, llvm::BasicBlock *bb_success, llvm::BasicBlock *bb_fail){
	cond.compare();
	cond.jump(bb_success, bb_fail);
}
inline void jump(LLVMBuilders &cc, Condition<LLVMBuilders> cond, llvm::BasicBlock *bb_success){
	llvm::BasicBlock *bb_current = cc.ir.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_fallthrough = llvm::BasicBlock::Create(cc.ir.getContext(), "fallthrough", fn);

	cond.compare();
	cond.jump(bb_success, bb_fallthrough);
	// simulate implicit fallthrough
	cc.ir.SetInsertPoint(bb_fallthrough);
}


inline void ret(LLVMBuilders &cc){
	cc.ir.CreateRetVoid();
}
template<typename FnPtr, typename VReg>
inline void ret(Function<runtimellvmjit,FnPtr> &fn, VReg &reg){
	static_assert(std::is_same_v<typename Function<runtimellvmjit,FnPtr>::return_type, typename VReg::value_type>, "incompatible return types");
	fn.cc.ir.CreateRet(reg.load());
}
template<typename FnPtr, typename VReg>
inline void ret(InternalFunction<runtimellvmjit,FnPtr> &fn, VReg &reg){
	static_assert(std::is_same_v<typename InternalFunction<runtimellvmjit,FnPtr>::return_type, typename VReg::value_type>, "incompatible return types");
	fn.cc.ir.CreateRet(reg.load());
}


template<typename Fn>
void if_then(LLVMBuilders &cc, Condition<LLVMBuilders> cond, Fn &&then){
	llvm::BasicBlock *bb_current = cc.ir.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_then = llvm::BasicBlock::Create(cc.ir.getContext(), "then", fn);
	llvm::BasicBlock *bb_afterthen = llvm::BasicBlock::Create(cc.ir.getContext(), "afterthen", fn);
	// check
	jump(cc, cond, bb_then, bb_afterthen); // if not jump over

	cc.ir.SetInsertPoint(bb_then);
	then();
	// if then() already emitted a terminator (e.g., early exit with ret), don't emit branch
	// sometimes there is a cleanupret, which is a terminator but should be ignored
	//if(!cc.ir.GetInsertBlock()->back().isTerminator()){
	// just check for the return instruction as we actually just care about an early exit for now
	if(cc.ir.GetInsertBlock()->back().getOpcode() != llvm::Instruction::Ret){
		jump(cc, bb_afterthen);
#if 0
	}else{
		//DEBUG
		llvm::BasicBlock *bb = cc.ir.GetInsertBlock();
		printf("found terminator in BB %s: %s (%u)\n", bb->getName().data(), bb->back().getOpcodeName(), bb->back().getOpcode());
#endif
	}

	// label after then branch
	cc.ir.SetInsertPoint(bb_afterthen);
}

template<typename Then, typename Else>
void if_then_else(LLVMBuilders &cc, Condition<LLVMBuilders> cond, Then &&then, Else &&else_){
	llvm::BasicBlock *bb_current = cc.ir.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_then = llvm::BasicBlock::Create(cc.ir.getContext(), "then", fn);
	llvm::BasicBlock *bb_else = llvm::BasicBlock::Create(cc.ir.getContext(), "else", fn);
	llvm::BasicBlock *bb_after = nullptr;
	// check
	jump(cc, cond, bb_then, bb_else); // if not jump to else

	cc.ir.SetInsertPoint(bb_then);
	then();
	// if then() already emitted a ret, don't emit branch
	// see if_then() for details
	if(cc.ir.GetInsertBlock()->back().getOpcode() != llvm::Instruction::Ret){
		bb_after = llvm::BasicBlock::Create(cc.ir.getContext(), "after", fn);
		jump(cc, bb_after);
	}

	cc.ir.SetInsertPoint(bb_else);
	else_();
	// if else_() already emitted a ret, don't emit branch
	if(cc.ir.GetInsertBlock()->back().getOpcode() != llvm::Instruction::Ret){
		if(bb_after == nullptr){
			bb_after = llvm::BasicBlock::Create(cc.ir.getContext(), "after", fn);
		}
		jump(cc, bb_after);
	}

	// label after then branch
	if(bb_after){
		cc.ir.SetInsertPoint(bb_after);
	}
}


template<typename Fn>
void loop_while(LLVMBuilders &cc, Condition<LLVMBuilders> cond, Fn &&body){
	llvm::BasicBlock *bb_current = cc.ir.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(cc.ir.getContext(), "loop", fn);
	llvm::BasicBlock *bb_afterloop = llvm::BasicBlock::Create(cc.ir.getContext(), "afterloop", fn);

	// check if even one iteration
	jump(cc, cond, bb_loop, bb_afterloop); // if not jump over

	// loop
	cc.ir.SetInsertPoint(bb_loop);
		body();
	jump(cc, cond, bb_loop, bb_afterloop);

	// label after loop body
	cc.ir.SetInsertPoint(bb_afterloop);
}

template<typename Fn>
void do_while(LLVMBuilders &cc, Fn &&body, Condition<LLVMBuilders> cond){
	llvm::BasicBlock *bb_current = cc.ir.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(cc.ir.getContext(), "loop", fn);
	llvm::BasicBlock *bb_afterloop = llvm::BasicBlock::Create(cc.ir.getContext(), "afterloop", fn);

	// no checking, but still have to jump to basic block containing the loop
	jump(cc, bb_loop);

	// loop
	cc.ir.SetInsertPoint(bb_loop);
		body();
	jump(cc, cond, bb_loop, bb_afterloop);

	// label after loop body
	cc.ir.SetInsertPoint(bb_afterloop);
}

template<class Ptr, typename Body>
void for_each(LLVMBuilders &cc, Ptr &begin, const Ptr &end, Body &&body, const char *file=__builtin_FILE(), int line=__builtin_LINE()){
	llvm::BasicBlock *bb_current = cc.ir.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(cc.ir.getContext(), "foreach", fn);
	llvm::BasicBlock *bb_after = llvm::BasicBlock::Create(cc.ir.getContext(), "after", fn);

	cc.ir.SetCurrentDebugLocation(llvm::DebugLoc::get(line, 0, cc.debugScope));
	// check if even one iteration
	jump(cc, begin == end, bb_after, bb_loop);

	// loop over all elements
	cc.ir.SetInsertPoint(bb_loop);
		typename Ptr::mem_type vr_ele = *begin;
		body(vr_ele);
		++begin;
	jump(cc, begin != end, bb_loop, bb_after);

	// label after loop body
	cc.ir.SetInsertPoint(bb_after);
}

template<class T, typename Fn>
void for_each(LLVMBuilders &cc, const T &container, Fn &&body){
	llvm::BasicBlock *bb_current = cc.ir.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(cc.ir.getContext(), "foreach", fn);
	llvm::BasicBlock *bb_after = llvm::BasicBlock::Create(cc.ir.getContext(), "after", fn);

	auto begin = container.begin();
	auto end = container.end();

	// check if even one iteration
	jump(cc, begin == end, bb_after, bb_loop);

	// loop over all elements
	cc.ir.SetInsertPoint(bb_loop);
		auto vr_ele = *begin;
		body(vr_ele);
		++begin;
	jump(cc, begin != end, bb_loop, bb_after);

	// label after loop body
	cc.ir.SetInsertPoint(bb_after);
}


// calling function outside of generated code
template<typename R, typename ...Args>
std::conditional_t<std::is_void_v<R>, void, reg_type<LLVMBuilders,R>>
FunctionCall(LLVMBuilders &cc, R(*fnptr)(Args...), const char *name, const wrapper_type<LLVMBuilders,Args>&... arguments){
	llvm::Module *currentModule = cc.ir.GetInsertBlock()->getModule();
	llvm::Function *fn = currentModule->getFunction(name);
	if(!fn){
		// first call to external function
		llvm::LLVMContext &ctx = currentModule->getContext();
		llvm::FunctionType *func_type = llvm::FunctionType::get(
			getLLVMType<std::remove_cv_t<R>>(ctx),
			{(getLLVMType<std::remove_cv_t<Args>>(ctx))...},
			false
		);
		// add function to module
		fn = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name, currentModule);
		fn->setCallingConv(llvm::CallingConv::C);

		// adds address of funtion pointer to metadata
		// in Function::finalize it is added as absolute symbols
		llvm::MDNode *md = llvm::MDNode::get(ctx,
			llvm::ConstantAsMetadata::get(
				llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), (uint64_t)fnptr)
			)
		);
		fn->setMetadata("coat.fnptr", md);
	}
	// call
	llvm::CallInst *call_inst = cc.ir.CreateCall(fn, { arguments.load()... });

	if constexpr(!std::is_void_v<R>){
		// return value
		reg_type<LLVMBuilders,R> ret(cc, "callreturn");
		ret = call_inst;
		return ret;
	}
}

// calling function in generated code
template<typename R, typename ...Args>
std::conditional_t<std::is_void_v<R>, void, reg_type<LLVMBuilders,R>>
FunctionCall(LLVMBuilders &cc, const Function<runtimellvmjit,R(*)(Args...)> &func, const wrapper_type<LLVMBuilders,Args>&... arguments){
	// call
	llvm::CallInst *call_inst = cc.ir.CreateCall(func.func, { arguments.load()... });

	if constexpr(!std::is_void_v<R>){
		// return value
		reg_type<LLVMBuilders,R> ret(cc, "callreturn");
		ret = call_inst;
		return ret;
	}
}

// calling internal function inside generated code
template<typename R, typename ...Args>
std::conditional_t<std::is_void_v<R>, void, reg_type<LLVMBuilders,R>>
FunctionCall(LLVMBuilders &cc, const InternalFunction<runtimellvmjit,R(*)(Args...)> &func, const wrapper_type<LLVMBuilders,Args>&... arguments){
	// call
	llvm::CallInst *call_inst = cc.ir.CreateCall(func.func, { arguments.load()... });

	if constexpr(!std::is_void_v<R>){
		// return value
		reg_type<LLVMBuilders,R> ret(cc, "callreturn");
		ret = call_inst;
		return ret;
	}
}


// pointer difference in bytes, no pointer arithmetic (used by Ptr operators)
template<typename T>
Value<LLVMBuilders,size_t> distance(LLVMBuilders &cc, Ptr<LLVMBuilders,Value<LLVMBuilders,T>> &beg, Ptr<LLVMBuilders,Value<LLVMBuilders,T>> &end){
	Value<LLVMBuilders,size_t> vr_ret(cc, "distance");
	llvm::Value *int_beg = cc.ir.CreatePtrToInt(beg.load(), llvm::Type::getInt64Ty(cc.ir.getContext()));
	llvm::Value *int_end = cc.ir.CreatePtrToInt(end.load(), llvm::Type::getInt64Ty(cc.ir.getContext()));
	llvm::Value *bytes = cc.ir.CreateSub(int_end, int_beg);
	vr_ret.store(bytes);
	return vr_ret;
}


} // namespace

#endif
