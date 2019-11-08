#ifndef COAT_ASMJIT_FUNCTION_H_
#define COAT_ASMJIT_FUNCTION_H_

#include "../runtimeasmjit.h"
#ifndef NDEBUG
#	include <asmjit-utilities/perf/perfcompiler.h>
#endif

#include <tuple> // apply
#include <cstdio>


namespace coat {

template<typename R, typename ...Args>
struct Function<runtimeasmjit,R(*)(Args...)>{
	using CC = runtimeasmjit;
	using F = ::asmjit::x86::Compiler;
	using func_type = R (*)(Args...);
	using return_type = R;

	runtimeasmjit &asmrt;
	::asmjit::CodeHolder code;
#ifdef NDEBUG
	::asmjit::x86::Compiler cc;
#else
	PerfCompiler cc;
#endif

	::asmjit::FileLogger logger;

	const char *funcName;
	::asmjit::FuncNode *funcNode;

	Function(runtimeasmjit &asmrt, const char *funcName="func") : asmrt(asmrt), cc(nullptr), funcName(funcName) {
		code.init(asmrt.rt.codeInfo());
		code.setErrorHandler(&asmrt.errorHandler);
		code.attach(&cc);

		funcNode = cc.addFunc(::asmjit::FuncSignatureT<R,Args...>());
	}
	Function(const Function &other) = delete;

	void enableCodeDump(FILE *fd=stdout){
		logger.setFlags(asmjit::FormatOptions::kFlagHexOffsets);
		logger.setFile(fd);
		code.setLogger(&logger);
	}

	template<typename FuncSig>
	InternalFunction<runtimeasmjit,FuncSig> addFunction(const char* /* ignore function name */){
		return InternalFunction<runtimeasmjit,FuncSig>(cc);
	}

	template<class IFunc>
	void startNextFunction(const IFunc &internalCall){
		// close previous function
		cc.endFunc();
		// start passed function
		cc.addFunc(internalCall.funcNode);
	}

	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(Names... names) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough or too many names specified");
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

		cc.endFunc();
		cc.finalize();

		::asmjit::Error err = asmrt.rt.add(&fn, &code);
		if(err){
			fprintf(stderr, "runtime add failed with CodeCompiler\n");
			std::exit(1);
		}
#ifdef PROFILING
		// dump generated code for profiling with perf
		//asmrt.jd.addCodeSegment(funcName, (void*)fn, code.codeSize());
		cc.addCodeSegment(funcName, (void*)fn, code.codeSize());
#endif
		return fn;
	}

	operator const ::asmjit::x86::Compiler&() const { return cc; }
	operator       ::asmjit::x86::Compiler&()       { return cc; }
};


template<typename R, typename ...Args>
struct InternalFunction<runtimeasmjit,R(*)(Args...)> {
	using F = ::asmjit::x86::Compiler;
	using func_type = R (*)(Args...);
	using return_type = R;

	::asmjit::x86::Compiler &cc;
	::asmjit::FuncNode *funcNode;

	InternalFunction(::asmjit::x86::Compiler &cc) : cc(cc) {
		funcNode = cc.newFunc(::asmjit::FuncSignatureT<R,Args...>());
	}
	InternalFunction(const InternalFunction &other) : cc(other.cc), funcNode(other.funcNode) {}


	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(Names... names) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough or too many names specified");
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

	operator const ::asmjit::x86::Compiler&() const { return cc; }
	operator       ::asmjit::x86::Compiler&()       { return cc; }
};


} // namespace

#endif
