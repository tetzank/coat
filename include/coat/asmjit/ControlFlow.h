#ifndef COAT_ASMJIT_CONTROLFLOW_H_
#define COAT_ASMJIT_CONTROLFLOW_H_


namespace coat {

inline void jump(::asmjit::x86::Compiler &cc, ::asmjit::Label label){
	cc.jmp(label);
}
inline void jump(::asmjit::x86::Compiler &, const Condition<::asmjit::x86::Compiler> &cond, ::asmjit::Label label){
	cond.compare();
	cond.jump(label);
}


inline void ret(::asmjit::x86::Compiler &cc){
	cc.ret();
}
template<typename FnPtr, typename VReg>
inline void ret(Function<runtimeasmjit,FnPtr> &ctx, VReg &reg){
	static_assert(std::is_same_v<typename Function<runtimeasmjit,FnPtr>::return_type, typename VReg::value_type>, "incompatible return types");
	ctx.cc.ret(reg);
}
template<typename FnPtr, typename VReg>
inline void ret(InternalFunction<runtimeasmjit,FnPtr> &ctx, VReg &reg){
	static_assert(std::is_same_v<typename InternalFunction<runtimeasmjit,FnPtr>::return_type, typename VReg::value_type>, "incompatible return types");
	ctx.cc.ret(reg);
}


template<typename Fn>
void if_then(::asmjit::x86::Compiler &cc, Condition<::asmjit::x86::Compiler> cond, Fn &&then){
	::asmjit::Label l_exit = cc.newLabel();
	// check
	jump(cc, !cond, l_exit); // if not jump over
	then();
	// label after then branch
	cc.bind(l_exit);
}

template<typename Then, typename Else>
void if_then_else(::asmjit::x86::Compiler &cc, Condition<::asmjit::x86::Compiler> cond, Then &&then, Else &&else_){
	::asmjit::Label l_else = cc.newLabel();
	::asmjit::Label l_exit = cc.newLabel();
	// check
	jump(cc, !cond, l_else); // if not jump to else
	then();
	jump(cc, l_exit);
	cc.bind(l_else);
	else_();
	// label after then branch
	cc.bind(l_exit);
}


template<typename Fn>
void loop_while(::asmjit::x86::Compiler &cc, Condition<::asmjit::x86::Compiler> cond, Fn &&body){
	::asmjit::Label l_loop = cc.newLabel();
	::asmjit::Label l_exit = cc.newLabel();

	// check if even one iteration
	jump(cc, !cond, l_exit); // if not jump over

	// loop
	cc.bind(l_loop);
		body();
	jump(cc, cond, l_loop);

	// label after loop body
	cc.bind(l_exit);
}

template<typename Fn>
void do_while(::asmjit::x86::Compiler &cc, Fn &&body, Condition<::asmjit::x86::Compiler> cond){
	::asmjit::Label l_loop = cc.newLabel();

	// no checking if even one iteration

	// loop
	cc.bind(l_loop);
		body();
	jump(cc, cond, l_loop);
}

template<class Ptr, typename Fn>
void for_each(::asmjit::x86::Compiler &cc, Ptr &begin, Ptr &end, Fn &&body){
	::asmjit::Label l_loop = cc.newLabel();
	::asmjit::Label l_exit = cc.newLabel();

	// check if even one iteration
	jump(cc, begin == end, l_exit);

	// loop over all elements
	cc.bind(l_loop);
		typename Ptr::mem_type vr_ele = *begin;
		body(vr_ele);
		++begin;
	jump(cc, begin != end, l_loop);

	// label after loop body
	cc.bind(l_exit);
}

template<class T, typename Fn>
void for_each(::asmjit::x86::Compiler &cc, const T &container, Fn &&body){
	::asmjit::Label l_loop = cc.newLabel();
	::asmjit::Label l_exit = cc.newLabel();

	auto begin = container.begin();
	auto end = container.end();

	// check if even one iteration
	jump(cc, begin == end, l_exit);

	// loop over all elements
	cc.bind(l_loop);
		auto vr_ele = *begin;
		body(vr_ele);
		++begin;
	jump(cc, begin != end, l_loop);

	// label after loop body
	cc.bind(l_exit);
}


// calling function pointer, from generated code to C++ function
template<typename R, typename ...Args>
std::conditional_t<std::is_void_v<R>, void, reg_type<::asmjit::x86::Compiler,R>>
FunctionCall(::asmjit::x86::Compiler &cc, R(*fnptr)(Args...), const char*, reg_type<::asmjit::x86::Compiler,Args>... arguments){
	if constexpr(std::is_void_v<R>){
		::asmjit::FuncCallNode *c = cc.call((uint64_t)(void*)fnptr, ::asmjit::FuncSignatureT<R,Args...>(::asmjit::CallConv::kIdHost));
		int index=0;
		// function parameters
		// (c->setArg(0, arguments_0), ... ; 
		((c->setArg(index++, arguments)), ...);
	}else{
		reg_type<::asmjit::x86::Compiler,R> ret(cc, "");
		::asmjit::FuncCallNode *c = cc.call((uint64_t)(void*)fnptr, ::asmjit::FuncSignatureT<R,Args...>(::asmjit::CallConv::kIdHost));
		int index=0;
		// function parameters
		// (c->setArg(0, arguments_0), ... ; 
		((c->setArg(index++, arguments)), ...);
		// return value
		c->setRet(0, ret);
		return ret;
	}
}

// calling generated function
template<typename R, typename ...Args>
std::conditional_t<std::is_void_v<R>, void, reg_type<::asmjit::x86::Compiler,R>>
FunctionCall(::asmjit::x86::Compiler &cc, Function<runtimeasmjit,R(*)(Args...)> &func, reg_type<::asmjit::x86::Compiler,Args>... arguments){
	if constexpr(std::is_void_v<R>){
		::asmjit::FuncCallNode *c = cc.call(func.funcNode->label(), ::asmjit::FuncSignatureT<R,Args...>(::asmjit::CallConv::kIdHost));
		int index=0;
		// function parameters
		// (c->setArg(0, arguments_0), ... ; 
		((c->setArg(index++, arguments)), ...);
	}else{
		reg_type<::asmjit::x86::Compiler,R> ret(cc, "");
		::asmjit::FuncCallNode *c = cc.call(func.funcNode->label(), ::asmjit::FuncSignatureT<R,Args...>(::asmjit::CallConv::kIdHost));
		int index=0;
		// function parameters
		// (c->setArg(0, arguments_0), ... ; 
		((c->setArg(index++, arguments)), ...);
		// return value
		c->setRet(0, ret);
		return ret;
	}
}

// calling internal function inside generated code
template<typename R, typename ...Args>
std::conditional_t<std::is_void_v<R>, void, reg_type<::asmjit::x86::Compiler,R>>
FunctionCall(::asmjit::x86::Compiler &cc, InternalFunction<runtimeasmjit,R(*)(Args...)> &func, reg_type<::asmjit::x86::Compiler,Args>... arguments){
	if constexpr(std::is_void_v<R>){
		::asmjit::FuncCallNode *c = cc.call(func.funcNode->label(), ::asmjit::FuncSignatureT<R,Args...>(::asmjit::CallConv::kIdHost));
		int index=0;
		// function parameters
		// (c->setArg(0, arguments_0), ... ; 
		((c->setArg(index++, arguments)), ...);
	}else{
		reg_type<::asmjit::x86::Compiler,R> ret(cc, "");
		::asmjit::FuncCallNode *c = cc.call(func.funcNode->label(), ::asmjit::FuncSignatureT<R,Args...>(::asmjit::CallConv::kIdHost));
		int index=0;
		// function parameters
		// (c->setArg(0, arguments_0), ... ; 
		((c->setArg(index++, arguments)), ...);
		// return value
		c->setRet(0, ret);
		return ret;
	}
}


// pointer difference in bytes, no pointer arithmetic (used by Ptr operators)
template<typename T>
Value<::asmjit::x86::Compiler,size_t> distance(::asmjit::x86::Compiler &cc, Ptr<::asmjit::x86::Compiler,Value<::asmjit::x86::Compiler,T>> &beg, Ptr<::asmjit::x86::Compiler,Value<::asmjit::x86::Compiler,T>> &end){
	Value<::asmjit::x86::Compiler,size_t> vr_ret(cc, "distance");
	cc.mov(vr_ret, end);
	cc.sub(vr_ret, beg);
	return vr_ret;
}

} // namespace

#endif
