#ifndef COAT_ASMJIT_FUNCTION_H_
#define COAT_ASMJIT_FUNCTION_H_


#include "../runtimeasmjit.h"


namespace coat {

template<typename R, typename ...Args>
struct Function<runtimeasmjit,R(*)(Args...)>{
	using F = ::asmjit::x86::Compiler;
	using func_type = R (*)(Args...);
	using return_type = R;

	::asmjit::CodeHolder code;
	::asmjit::x86::Compiler cc;

	::asmjit::FileLogger logger;

	Function(runtimeasmjit *runtime){
		code.init(runtime->rt.codeInfo());
#if 0
		logger.setFile(stdout);
		code.setLogger(&logger);
#endif
		code.setErrorHandler(&runtime->errorHandler);
		code.attach(&cc);

		cc.addFunc(::asmjit::FuncSignatureT<R,Args...>());
	}
	Function(const Function &other) = delete;


	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(Names... names) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough names specified");
		// create all parameter wrapper objects in a tuple
		std::tuple<wrapper_type<F,Args>...> ret { wrapper_type<F,Args>(cc, names)... };
		// get argument value and put it in wrapper object
		std::apply(
			[&](auto &&...args){
				int idx=0;
				// cc.setArg(0, tuple_at_0), cc.setArg(1, tuple_at_1), ... ;
				((cc.setArg(idx++, args)), ...);
			},
			ret
		);
		return ret;
	}

	//HACK: trying factory
	template<typename T>
	Value<::asmjit::x86::Compiler,T> getValue(const char *name="") {
		return Value<::asmjit::x86::Compiler,T>(cc, name);
	}

	func_type finalize(
		runtimeasmjit *runtime,
#ifdef PROFILING
		const char *fname="COAT_Function_AsmJit"
#else
		const char * =""
#endif
	){
		func_type fn;

		cc.endFunc();
		cc.finalize();

		::asmjit::Error err = runtime->rt.add(&fn, &code);
		if(err){
			fprintf(stderr, "runtime add failed with CodeCompiler\n");
			std::exit(1);
		}
#ifdef PROFILING
		// dump generated code for profiling with perf
		runtime->jd.addCodeSegment(fname, (void*)fn, code.codeSize());
#endif
		return fn;
	}

	operator const ::asmjit::x86::Compiler&() const { return cc; }
	operator       ::asmjit::x86::Compiler&()       { return cc; }
};

} // namespace

#endif
