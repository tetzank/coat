#ifndef LLVMJIT_CONTROLFLOW_H_
#define LLVMJIT_CONTROLFLOW_H_



inline void ret(::llvm::IRBuilder<> &builder){
	builder.CreateRetVoid();
}
template<typename FnPtr, typename VReg>
inline void ret(Function<runtimellvmjit,FnPtr> &ctx, VReg &reg){
	static_assert(std::is_same_v<typename Function<runtimellvmjit,FnPtr>::return_type, typename VReg::value_type>, "incompatible return types");
	ctx.builder.CreateRet(reg.load());
}




#endif
