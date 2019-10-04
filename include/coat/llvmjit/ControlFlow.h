#ifndef COAT_LLVMJIT_CONTROLFLOW_H_
#define COAT_LLVMJIT_CONTROLFLOW_H_


namespace coat {

inline void jump(llvm::IRBuilder<> &cc, llvm::BasicBlock *bb_dest){
	cc.CreateBr(bb_dest);
}
inline void jump(llvm::IRBuilder<> &cc, Condition<::llvm::IRBuilder<>> cond, llvm::BasicBlock *bb_success, llvm::BasicBlock *bb_fail){
	cond.compare();
	cond.jump(bb_success, bb_fail);
}


inline void ret(::llvm::IRBuilder<> &cc){
	cc.CreateRetVoid();
}
template<typename FnPtr, typename VReg>
inline void ret(Function<runtimellvmjit,FnPtr> &ctx, VReg &reg){
	static_assert(std::is_same_v<typename Function<runtimellvmjit,FnPtr>::return_type, typename VReg::value_type>, "incompatible return types");
	ctx.cc.CreateRet(reg.load());
}
template<typename FnPtr, typename VReg>
inline void ret(InternalFunction<runtimellvmjit,FnPtr> &ctx, VReg &reg){
	static_assert(std::is_same_v<typename InternalFunction<runtimellvmjit,FnPtr>::return_type, typename VReg::value_type>, "incompatible return types");
	ctx.cc.CreateRet(reg.load());
}


template<typename Fn>
void if_then(llvm::IRBuilder<> &cc, Condition<::llvm::IRBuilder<>> cond, Fn &&then){
	llvm::BasicBlock *bb_current = cc.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_then = llvm::BasicBlock::Create(cc.getContext(), "then", fn);
	llvm::BasicBlock *bb_afterthen = llvm::BasicBlock::Create(cc.getContext(), "afterthen", fn);
	// check
	jump(cc, cond, bb_then, bb_afterthen); // if not jump over

	cc.SetInsertPoint(bb_then);
	then();
	// if then() already emitted a terminator (e.g., early exit with ret), don't emit branch
	// sometimes there is a cleanupret, which is a terminator but should be ignored
	//if(!cc.GetInsertBlock()->back().isTerminator()){
	// just check for the return instruction as we actually just care about an early exit for now
	if(cc.GetInsertBlock()->back().getOpcode() != llvm::Instruction::Ret){
		jump(cc, bb_afterthen);
#if 0
	}else{
		//DEBUG
		llvm::BasicBlock *bb = cc.GetInsertBlock();
		printf("found terminator in BB %s: %s (%u)\n", bb->getName().data(), bb->back().getOpcodeName(), bb->back().getOpcode());
#endif
	}

	// label after then branch
	cc.SetInsertPoint(bb_afterthen);
}

template<typename Then, typename Else>
void if_then_else(llvm::IRBuilder<> &cc, Condition<::llvm::IRBuilder<>> cond, Then &&then, Else &&else_){
	llvm::BasicBlock *bb_current = cc.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_then = llvm::BasicBlock::Create(cc.getContext(), "then", fn);
	llvm::BasicBlock *bb_else = llvm::BasicBlock::Create(cc.getContext(), "else", fn);
	llvm::BasicBlock *bb_after = nullptr;
	// check
	jump(cc, cond, bb_then, bb_else); // if not jump to else

	cc.SetInsertPoint(bb_then);
	then();
	// if then() already emitted a ret, don't emit branch
	// see if_then() for details
	if(cc.GetInsertBlock()->back().getOpcode() != llvm::Instruction::Ret){
		bb_after = llvm::BasicBlock::Create(cc.getContext(), "after", fn);
		jump(cc, bb_after);
	}

	cc.SetInsertPoint(bb_else);
	else_();
	// if else_() already emitted a ret, don't emit branch
	if(cc.GetInsertBlock()->back().getOpcode() != llvm::Instruction::Ret){
		if(bb_after == nullptr){
			bb_after = llvm::BasicBlock::Create(cc.getContext(), "after", fn);
		}
		jump(cc, bb_after);
	}

	// label after then branch
	if(bb_after){
		cc.SetInsertPoint(bb_after);
	}
}


template<typename Fn>
void loop_while(llvm::IRBuilder<> &cc, Condition<::llvm::IRBuilder<>> cond, Fn &&body){
	llvm::BasicBlock *bb_current = cc.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(cc.getContext(), "loop", fn);
	llvm::BasicBlock *bb_afterloop = llvm::BasicBlock::Create(cc.getContext(), "afterloop", fn);

	// check if even one iteration
	jump(cc, cond, bb_loop, bb_afterloop); // if not jump over

	// loop
	cc.SetInsertPoint(bb_loop);
		body();
	jump(cc, cond, bb_loop, bb_afterloop);

	// label after loop body
	cc.SetInsertPoint(bb_afterloop);
}

template<typename Fn>
void do_while(llvm::IRBuilder<> &cc, Fn &&body, Condition<::llvm::IRBuilder<>> cond){
	llvm::BasicBlock *bb_current = cc.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(cc.getContext(), "loop", fn);
	llvm::BasicBlock *bb_afterloop = llvm::BasicBlock::Create(cc.getContext(), "afterloop", fn);

	// no checking, but still have to jump to basic block containing the loop
	jump(cc, bb_loop);

	// loop
	cc.SetInsertPoint(bb_loop);
		body();
	jump(cc, cond, bb_loop, bb_afterloop);

	// label after loop body
	cc.SetInsertPoint(bb_afterloop);
}

template<class Ptr, typename Fn>
void for_each(llvm::IRBuilder<> &cc, Ptr &begin, const Ptr &end, Fn &&body){
	llvm::BasicBlock *bb_current = cc.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(cc.getContext(), "foreach", fn);
	llvm::BasicBlock *bb_after = llvm::BasicBlock::Create(cc.getContext(), "after", fn);

	// check if even one iteration
	jump(cc, begin == end, bb_after, bb_loop);

	// loop over all elements
	cc.SetInsertPoint(bb_loop);
		typename Ptr::mem_type vr_ele = *begin;
		body(vr_ele);
		++begin;
	jump(cc, begin != end, bb_loop, bb_after);

	// label after loop body
	cc.SetInsertPoint(bb_after);
}

template<class T, typename Fn>
void for_each(llvm::IRBuilder<> &cc, const T &container, Fn &&body){
	llvm::BasicBlock *bb_current = cc.GetInsertBlock();
	llvm::Function *fn = bb_current->getParent();
	llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(cc.getContext(), "foreach", fn);
	llvm::BasicBlock *bb_after = llvm::BasicBlock::Create(cc.getContext(), "after", fn);

	auto begin = container.begin();
	auto end = container.end();

	// check if even one iteration
	jump(cc, begin == end, bb_after, bb_loop);

	// loop over all elements
	cc.SetInsertPoint(bb_loop);
		auto vr_ele = *begin;
		body(vr_ele);
		++begin;
	jump(cc, begin != end, bb_loop, bb_after);

	// label after loop body
	cc.SetInsertPoint(bb_after);
}


//FIXME: requires that the function is exported as a dynamic symbol
//       on linux this is only the case when linking a shared library
//       for an executable, -rdynamic flag has to be used during linking

// calling function outside of generated code
template<typename R, typename ...Args>
std::conditional_t<std::is_void_v<R>, void, reg_type<::llvm::IRBuilder<>,R>>
FunctionCall(llvm::IRBuilder<> &cc, R(*fnptr)(Args...), const char *name, reg_type<::llvm::IRBuilder<>,Args>... arguments){
	llvm::Module *currentModule = cc.GetInsertBlock()->getModule();
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
	}
	// call
	llvm::CallInst *call_inst = cc.CreateCall(fn, { arguments.load()... });

	if constexpr(!std::is_void_v<R>){
		// return value
		reg_type<::llvm::IRBuilder<>,R> ret(cc, "callreturn");
		ret = call_inst;
		return ret;
	}
}

// calling function in generated code
template<typename R, typename ...Args>
std::conditional_t<std::is_void_v<R>, void, reg_type<::llvm::IRBuilder<>,R>>
FunctionCall(llvm::IRBuilder<> &cc, Function<runtimellvmjit,R(*)(Args...)> &func, reg_type<::llvm::IRBuilder<>,Args>... arguments){
	// call
	llvm::CallInst *call_inst = cc.CreateCall(func.func, { arguments.load()... });

	if constexpr(!std::is_void_v<R>){
		// return value
		reg_type<::llvm::IRBuilder<>,R> ret(cc, "callreturn");
		ret = call_inst;
		return ret;
	}
}

// calling internal function inside generated code
template<typename R, typename ...Args>
std::conditional_t<std::is_void_v<R>, void, reg_type<::llvm::IRBuilder<>,R>>
FunctionCall(llvm::IRBuilder<> &cc, InternalFunction<runtimellvmjit,R(*)(Args...)> &func, reg_type<::llvm::IRBuilder<>,Args>... arguments){
	// call
	llvm::CallInst *call_inst = cc.CreateCall(func.func, { arguments.load()... });

	if constexpr(!std::is_void_v<R>){
		// return value
		reg_type<::llvm::IRBuilder<>,R> ret(cc, "callreturn");
		ret = call_inst;
		return ret;
	}
}


// pointer difference in bytes, no pointer arithmetic (used by Ptr operators)
template<typename T>
Value<::llvm::IRBuilder<>,size_t> distance(::llvm::IRBuilder<> &cc, Ptr<::llvm::IRBuilder<>,Value<::llvm::IRBuilder<>,T>> &beg, Ptr<::llvm::IRBuilder<>,Value<::llvm::IRBuilder<>,T>> &end){
	Value<::llvm::IRBuilder<>,size_t> vr_ret(cc, "distance");
	llvm::Value *int_beg = cc.CreatePtrToInt(beg.load(), llvm::Type::getInt64Ty(cc.getContext()));
	llvm::Value *int_end = cc.CreatePtrToInt(end.load(), llvm::Type::getInt64Ty(cc.getContext()));
	llvm::Value *bytes = cc.CreateSub(int_end, int_beg);
	vr_ret.store(bytes);
	return vr_ret;
}


} // namespace

#endif
