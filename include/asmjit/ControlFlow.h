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
inline void ret(Function<runtimeAsmjit,FnPtr> &ctx, VReg &reg){
	static_assert(std::is_same_v<typename Function<runtimeAsmjit,FnPtr>::return_type, typename VReg::value_type>, "incompatible return types");
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

} // namespace

#endif
