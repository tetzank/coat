#ifndef COAT_LLVMJIT_FUNCTION_H_
#define COAT_LLVMJIT_FUNCTION_H_

#include "../runtimellvmjit.h"


namespace coat {

template<typename R, typename ...Args>
struct Function<runtimellvmjit,R(*)(Args...)>{
	using F = ::llvm::IRBuilder<>;
	using func_type = R (*)(Args...);
	using return_type = R;

	llvm::IRBuilder<> builder;

	Function(runtimellvmjit &jit) : builder(jit.context) {
		llvm::FunctionType *jit_func_type = llvm::FunctionType::get(
			getLLVMType<std::remove_cv_t<R>>(jit.context),
			{(getLLVMType<std::remove_cv_t<Args>>(jit.context))...},
			false
		);
		jit.reset(); //HACK
		jit.createFunction(jit_func_type, "func"); //FIXME: function name

		llvm::BasicBlock *bb_start = llvm::BasicBlock::Create(jit.context, "start", jit.jit_func);
		builder.SetInsertPoint(bb_start);
	}

	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(Names... names) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough names specified");
		// create all parameter wrapper objects in a tuple
		std::tuple<wrapper_type<F,Args>...> ret { wrapper_type<F,Args>(builder, names)... };
		// get argument value and put it in wrapper object
		std::apply(
			[&](auto &&...args){
				llvm::Function *fn = builder.GetInsertBlock()->getParent();
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
	Value<::llvm::IRBuilder<>,T> getValue(const char *name="") {
		return Value<::llvm::IRBuilder<>,T>(builder, name);
	}

	func_type finalize(runtimellvmjit &jit){
		func_type fn;

		jit.finalize();
		fn = (func_type) jit.getSymbolAddress("func");
		return fn;
	}

	operator const llvm::IRBuilder<>&() const { return builder; }
	operator       llvm::IRBuilder<>&()       { return builder; }
};

} // namespace

#endif
