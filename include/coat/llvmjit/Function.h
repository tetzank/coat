#ifndef COAT_LLVMJIT_FUNCTION_H_
#define COAT_LLVMJIT_FUNCTION_H_

#include "../runtimellvmjit.h"


namespace coat {

template<typename R, typename ...Args>
struct Function<runtimellvmjit,R(*)(Args...)>{
	using F = ::llvm::IRBuilder<>;
	using func_type = R (*)(Args...);
	using return_type = R;

	llvm::IRBuilder<> cc;

	const char *name;
	llvm::Function *func;

	Function(runtimellvmjit &jit, const char *name="func") : cc(jit.context), name(name) {
		llvm::FunctionType *jit_func_type = llvm::FunctionType::get(
			getLLVMType<std::remove_cv_t<R>>(jit.context),
			{(getLLVMType<std::remove_cv_t<Args>>(jit.context))...},
			false
		);
		jit.reset(); //HACK
		jit.createFunction(jit_func_type, name); // function name
		func = jit.jit_func; //FIXME: so hacky

		llvm::BasicBlock *bb_prolog = llvm::BasicBlock::Create(jit.context, "prolog", jit.jit_func);
		llvm::BasicBlock *bb_start = llvm::BasicBlock::Create(jit.context, "start", jit.jit_func);
		cc.SetInsertPoint(bb_prolog);
		cc.CreateBr(bb_start);

		cc.SetInsertPoint(bb_start);
	}

	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(Names... names) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough names specified");
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

	func_type finalize(runtimellvmjit &jit){
		func_type fn;

		jit.finalize();
		fn = (func_type) jit.getSymbolAddress(name);
		return fn;
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
