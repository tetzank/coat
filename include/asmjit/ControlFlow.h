#ifndef ASMJIT_CONTROLFLOW_H_
#define ASMJIT_CONTROLFLOW_H_



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

#endif
