#ifndef COAT_RUNTIMEASMJIT_HPP_
#define COAT_RUNTIMEASMJIT_HPP_

#include <asmjit/asmjit.h>
#if defined(PROFILING_ASSEMBLY) || defined(PROFILING_SOURCE)
#	include <asmjit-utilities/perf/jitdump.h>
#endif


namespace coat {

class MyErrorHandler : public asmjit::ErrorHandler{
public:
	void handleError(asmjit::Error /*err*/, const char *msg, asmjit::BaseEmitter * /*origin*/) override {
		fprintf(stderr, "ERROR: %s\n", msg);
	}
};


struct runtimeasmjit{
	asmjit::JitRuntime rt;
	MyErrorHandler errorHandler;
#if defined(PROFILING_ASSEMBLY) || defined(PROFILING_SOURCE)
	JitDump jd;

	runtimeasmjit(){
		jd.init();
	}
	~runtimeasmjit(){
		jd.close();
	}
#endif

	template<typename FnPtr>
	Function<runtimeasmjit,FnPtr> createFunction(const char *funcName="func"){
		return Function<runtimeasmjit,FnPtr>(*this, funcName);
	}
};

} // namespace

#endif
