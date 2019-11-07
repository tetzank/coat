#ifndef COAT_RUNTIMEASMJIT_HPP_
#define COAT_RUNTIMEASMJIT_HPP_

#include <asmjit/asmjit.h>
#ifdef PROFILING
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
#ifdef PROFILING
	JitDump jd;

	runtimeasmjit(){
		jd.init();
	}
	~runtimeasmjit(){
		jd.close();
	}
#endif
};

} // namespace

#endif
