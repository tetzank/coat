#ifndef ASMJIT_CONTROLFLOW_H_
#define ASMJIT_CONTROLFLOW_H_




template<typename FnPtr, typename VReg>
inline void ret(Function<runtimeAsmjit,FnPtr> &ctx, VReg &reg){
	static_assert(std::is_same_v<typename Function<runtimeAsmjit,FnPtr>::return_type, typename VReg::value_type>, "incompatible return types");
	ctx.cc.ret(reg);
}


#endif
