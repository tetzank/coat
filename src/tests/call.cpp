#include <cstdio>
#include <vector>
#include <numeric>

#include <coat/Function.h>
#include <coat/ControlFlow.h>


// function to be called, residing outside of generated code
int calc(int value){
	return value * 2;
}

int main(){
	// signature of the generated function
	using func_t = int (*)(int value);

#ifdef ENABLE_ASMJIT
	// initialize backend, AsmJit in this case
	coat::runtimeasmjit asmrt;
	// context object representing the generated function
	auto fn = asmrt.createFunction<func_t>();
#elif defined(ENABLE_LLVMJIT)
	// initialize LLVM backend
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	// context object representing the generated function
	auto fn = llvmrt.createFunction<func_t>();
#else
#	error "Neither AsmJit nor LLVM enabled"
#endif

	// start of the EDSL code describing the code of the generated function
	{
		// get function arguments as "meta-variables"
		auto [value] = fn.getArguments("value");
		
		// generate function call
		auto ret = coat::FunctionCall(fn, calc, "calc", value);

		// specify return value
		coat::ret(fn, ret);
	}
#ifdef ENABLE_LLVMJIT
	fn.printIR("call.ll");
	if(!fn.verify()){
		puts("verification failed. aborting.");
		exit(EXIT_FAILURE); //FIXME: better error handling
	}
#endif
	// finalize code generation and get function pointer to the generated function
	func_t foo = fn.finalize();

	// execute the generated function
	uint64_t result = foo(42);

	// print result
	uint64_t expected = calc(42);
	if(result == expected){
		printf("correct result: %lu\n", result);
		return 0;
	}else{
		printf("wrong result:\nresult: %lu; expected: %lu\n", result, expected);
		return -1;
	}
}
