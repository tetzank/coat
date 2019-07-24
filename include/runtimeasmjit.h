#ifndef RUNTIMEASMJIT_HPP_
#define RUNTIMEASMJIT_HPP_

#include <asmjit/asmjit.h>


class MyErrorHandler : public asmjit::ErrorHandler{
public:
	void handleError(asmjit::Error /*err*/, const char *msg, asmjit::BaseEmitter * /*origin*/) override {
		fprintf(stderr, "ERROR: %s\n", msg);
	}
};


struct runtimeAsmjit{
	asmjit::JitRuntime rt;
	MyErrorHandler errorHandler;
#ifdef PROFILING
	asmjit::JitDump jd;

	runtimeAsmjit(){
		jd.init();
	}
	~runtimeAsmjit(){
		jd.close();
	}
#endif
};

#endif
