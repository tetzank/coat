#ifndef COAT_LLVMJIT_FUNCTION_H_
#define COAT_LLVMJIT_FUNCTION_H_

#include "../runtimellvmjit.h"

#include <tuple> // apply


namespace coat {

template<typename R, typename ...Args>
struct Function<runtimellvmjit,R(*)(Args...)>{
	using CC = runtimellvmjit;
	using F = ::llvm::IRBuilder<>;
	using func_type = R (*)(Args...);
	using return_type = R;

	runtimellvmjit &jit;
	llvm::IRBuilder<> cc;

	const char *name;
	llvm::Function *func;

	Function(runtimellvmjit &jit, const char *name="func") : jit(jit), cc(jit.context), name(name) {
		llvm::FunctionType *jit_func_type = llvm::FunctionType::get(
			getLLVMType<std::remove_cv_t<R>>(jit.context),
			{(getLLVMType<std::remove_cv_t<Args>>(jit.context))...},
			false
		);
		jit.reset(); //HACK
		func = jit.createFunction(jit_func_type, name); // function name
		//FIXME: so hacky
		jit.jit_func = func;

		llvm::BasicBlock *bb_prolog = llvm::BasicBlock::Create(jit.context, "prolog", jit.jit_func);
		llvm::BasicBlock *bb_start = llvm::BasicBlock::Create(jit.context, "start", jit.jit_func);
		cc.SetInsertPoint(bb_prolog);
		cc.CreateBr(bb_start);

		cc.SetInsertPoint(bb_start);
	}
	Function(const Function &) = delete;


	template<typename FuncSig>
	InternalFunction<runtimellvmjit,FuncSig> addFunction(const char *name){
		return InternalFunction<runtimellvmjit,FuncSig>(jit, cc, name);
	}

	template<class IFunc>
	void startNextFunction(const IFunc &internalCall){
		jit.jit_func = internalCall.func;
		// start new basic block in internal function
		llvm::BasicBlock *bb_prolog = llvm::BasicBlock::Create(cc.getContext(), "prolog", internalCall.func);
		llvm::BasicBlock *bb_start = llvm::BasicBlock::Create(cc.getContext(), "start", internalCall.func);
		// start inserting here
		cc.SetInsertPoint(bb_prolog);
		cc.CreateBr(bb_start);
		cc.SetInsertPoint(bb_start);
	}


	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(Names... names) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough or too many names specified");
		// create all parameter wrapper objects in a tuple
		std::tuple<wrapper_type<F,Args>...> ret { wrapper_type<F,Args>(cc, names)... };
		// get argument value and put it in wrapper object
		std::apply(
			[&](auto &&...args){
				llvm::Function *fn = cc.GetInsertBlock()->getParent();
				llvm::Function::arg_iterator arguments = fn->arg_begin();
				// (tuple_at_0 = (llvm::Value*)args++), (tuple_at_1 = (llvm::Value*)args++), ... ;
				((args = (llvm::Value*)arguments++), ...);
			},
			ret
		);
		return ret;
	}

	//HACK: trying factory
	template<typename T>
	Value<F,T> getValue(const char *name="") {
		return Value<F,T>(cc, name);
	}
	// embed value in the generated code, returns wrapper initialized to this value
	template<typename T>
	wrapper_type<F,T> embedValue(T value, const char *name=""){
		return wrapper_type<F,T>(cc, value, name);
	}

	func_type finalize(){
		func_type fn;

		jit.finalize();
		fn = (func_type) jit.getSymbolAddress(name);
		return fn;
	}

	operator const llvm::IRBuilder<>&() const { return cc; }
	operator       llvm::IRBuilder<>&()       { return cc; }
};


template<typename R, typename ...Args>
struct InternalFunction<runtimellvmjit,R(*)(Args...)>{
	using F = ::llvm::IRBuilder<>;
	using func_type = R (*)(Args...);
	using return_type = R;

	llvm::IRBuilder<> &cc;
	const char *name;
	llvm::Function *func;

	InternalFunction(runtimellvmjit &jit, llvm::IRBuilder<> &cc, const char *name) : cc(cc), name(name) {
		llvm::FunctionType *func_type = llvm::FunctionType::get(
			getLLVMType<std::remove_cv_t<R>>(jit.context),
			{(getLLVMType<std::remove_cv_t<Args>>(jit.context))...},
			false
		);
		func = jit.createFunction(func_type, name, llvm::Function::InternalLinkage); // function name
	}
	InternalFunction(const InternalFunction &other) : cc(other.cc), name(other.name), func(other.func) {}


	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(Names... names) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough or too many names specified");
		// create all parameter wrapper objects in a tuple
		std::tuple<wrapper_type<F,Args>...> ret { wrapper_type<F,Args>(cc, names)... };
		// get argument value and put it in wrapper object
		std::apply(
			[&](auto &&...args){
				llvm::Function *fn = cc.GetInsertBlock()->getParent();
				llvm::Function::arg_iterator arguments = fn->arg_begin();
				// (tuple_at_0 = (llvm::Value*)args++), (tuple_at_1 = (llvm::Value*)args++), ... ;
				((args = (llvm::Value*)arguments++), ...);
			},
			ret
		);
		return ret;
	}

	operator const llvm::IRBuilder<>&() const { return cc; }
	operator       llvm::IRBuilder<>&()       { return cc; }
};


// helper function
static inline llvm::Value *allocateStackVariable(llvm::IRBuilder<> &cc, llvm::Type *ty, const char *name){
	llvm::BasicBlock *bb_current = cc.GetInsertBlock();
	llvm::BasicBlock &bb_prolog = bb_current->getParent()->getEntryBlock();
	llvm::BasicBlock::iterator it = bb_prolog.end();
	cc.SetInsertPoint(&bb_prolog, --it);
	llvm::Value *ret = cc.CreateAlloca(ty, nullptr, name);
	cc.SetInsertPoint(bb_current);
	return ret;
}

} // namespace

#endif
