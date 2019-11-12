#ifndef COAT_ASMJIT_CONTROLFLOW_H_
#define COAT_ASMJIT_CONTROLFLOW_H_


namespace coat {

inline void jump(::asmjit::x86::Compiler &cc, ::asmjit::Label label
#ifdef PROFILING_SOURCE
	, const char *file=__builtin_FILE(), int line=__builtin_LINE()
#endif
){
	cc.jmp(label);
#ifdef PROFILING_SOURCE
	((PerfCompiler&)cc).attachDebugLine(file, line);
#endif
}
inline void jump(::asmjit::x86::Compiler &, const Condition<::asmjit::x86::Compiler> &cond, ::asmjit::Label label
#ifdef PROFILING_SOURCE
	, const char *file=__builtin_FILE(), int line=__builtin_LINE()
#endif
){
#ifdef PROFILING_SOURCE
	cond.compare(file, line);
	cond.jump(label, file, line);
#else
	cond.compare();
	cond.jump(label);
#endif
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
void if_then(::asmjit::x86::Compiler &cc, Condition<::asmjit::x86::Compiler> cond, Fn &&then
#ifdef PROFILING_SOURCE
	, const char *file=__builtin_FILE(), int line=__builtin_LINE()
#endif
){
	::asmjit::Label l_exit = cc.newLabel();
	// check
#ifdef PROFILING_SOURCE
	jump(cc, !cond, l_exit, file, line); // if not jump over
#else
	jump(cc, !cond, l_exit); // if not jump over
#endif
	then();
	// label after then branch
	cc.bind(l_exit);
}

template<typename Then, typename Else>
void if_then_else(::asmjit::x86::Compiler &cc, Condition<::asmjit::x86::Compiler> cond, Then &&then, Else &&else_
#ifdef PROFILING_SOURCE
	, const char *file=__builtin_FILE(), int line=__builtin_LINE()
#endif
){
	::asmjit::Label l_else = cc.newLabel();
	::asmjit::Label l_exit = cc.newLabel();
	// check
#ifdef PROFILING_SOURCE
	jump(cc, !cond, l_else, file, line); // if not jump to else
#else
	jump(cc, !cond, l_else); // if not jump to else
#endif
	then();
#ifdef PROFILING_SOURCE
	jump(cc, l_exit, file, line);
#else
	jump(cc, l_exit);
#endif

	cc.bind(l_else);
	else_();
	// label after then branch
	cc.bind(l_exit);
}


template<typename Fn>
void loop_while(::asmjit::x86::Compiler &cc, Condition<::asmjit::x86::Compiler> cond, Fn &&body
#ifdef PROFILING_SOURCE
	, const char *file=__builtin_FILE(), int line=__builtin_LINE()
#endif
){
	::asmjit::Label l_loop = cc.newLabel();
	::asmjit::Label l_exit = cc.newLabel();

	// check if even one iteration
#ifdef PROFILING_SOURCE
	jump(cc, !cond, l_exit, file, line); // if not jump over
#else
	jump(cc, !cond, l_exit); // if not jump over
#endif

	// loop
	cc.bind(l_loop);
		body();
#ifdef PROFILING_SOURCE
	jump(cc, cond, l_loop, file, line);
#else
	jump(cc, cond, l_loop);
#endif

	// label after loop body
	cc.bind(l_exit);
}

template<typename Fn>
void do_while(::asmjit::x86::Compiler &cc, Fn &&body, Condition<::asmjit::x86::Compiler> cond
#ifdef PROFILING_SOURCE
	, const char *file=__builtin_FILE(), int line=__builtin_LINE()
#endif
){
	::asmjit::Label l_loop = cc.newLabel();

	// no checking if even one iteration

	// loop
	cc.bind(l_loop);
		body();
#ifdef PROFILING_SOURCE
	jump(cc, cond, l_loop, file, line);
#else
	jump(cc, cond, l_loop);
#endif
}

template<class Ptr, typename Fn>
void for_each(::asmjit::x86::Compiler &cc, Ptr &begin, Ptr &end, Fn &&body
#ifdef PROFILING_SOURCE
	, const char *file=__builtin_FILE(), int line=__builtin_LINE()
#endif
){
	::asmjit::Label l_loop = cc.newLabel();
	::asmjit::Label l_exit = cc.newLabel();

	// check if even one iteration
#ifdef PROFILING_SOURCE
	jump(cc, begin == end, l_exit, file, line);
#else
	jump(cc, begin == end, l_exit);
#endif

	// loop over all elements
	cc.bind(l_loop);
		typename Ptr::mem_type vr_ele = *begin;
		body(vr_ele);
#ifdef PROFILING_SOURCE
		begin += D<int>{1, file, line};
	jump(cc, begin != end, l_loop, file, line);
#else
		++begin;
	jump(cc, begin != end, l_loop);
#endif

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
FunctionCall(::asmjit::x86::Compiler &cc, R(*fnptr)(Args...), const char*, const wrapper_type<::asmjit::x86::Compiler,Args>&... arguments){
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
FunctionCall(::asmjit::x86::Compiler &cc, const Function<runtimeasmjit,R(*)(Args...)> &func, const wrapper_type<::asmjit::x86::Compiler,Args>&... arguments){
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
FunctionCall(::asmjit::x86::Compiler &cc, const InternalFunction<runtimeasmjit,R(*)(Args...)> &func, const wrapper_type<::asmjit::x86::Compiler,Args>&... arguments){
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
