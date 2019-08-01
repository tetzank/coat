#ifndef COAT_RUNTIMEASMJIT_HPP_
#define COAT_RUNTIMEASMJIT_HPP_

#include <asmjit/asmjit.h>


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
	asmjit::JitDump jd;

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
