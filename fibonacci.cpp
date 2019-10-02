#include <cstdlib>
#include <cstdio>
#include <cstdint>

#include <coat/Function.h>
#include <coat/ControlFlow.h>


uint32_t fib(uint32_t index){
	if(index < 2){
		return index;
	}else{
		return fib(index-1) + fib(index-2);
	}
}


template<class Fn>
void assemble_selfcall(Fn &fn){
	auto [index] = fn.getArguments("index");
	coat::if_then_else(fn, index < 2, [&]{
		coat::ret(fn, index);
	}, [&]{
		auto ret = coat::FunctionCall(fn, fn, index-1);
		ret += coat::FunctionCall(fn, fn, index-2);
		coat::ret(fn, ret);
	});
}

template<class Fn, typename Fnptr>
void assemble_crosscall(Fn &fn, Fnptr fnptr, const char *funcname){
	auto [index] = fn.getArguments("index");
	//FIXME: needs symbol name of function for LLVM, that is not exposed...
	auto ret = coat::FunctionCall(fn, fnptr, funcname, index);
	coat::ret(fn, ret);
}


int main(int argc, char *argv[]){
	if(argc < 2){
		puts("argument required: index in fibonacci sequence");
		return -1;
	}
	uint32_t index = atoi(argv[1]);
	uint32_t expected = fib(index);

	// init JIT backends
#ifdef ENABLE_ASMJIT
	coat::runtimeasmjit asmrt;
#endif
#ifdef ENABLE_LLVMJIT
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
#endif

	// signature of the generated function
	using func_t = uint32_t (*)(uint32_t index);

#ifdef ENABLE_ASMJIT
	{
		// context object representing the generated function
		coat::Function<coat::runtimeasmjit,func_t> fn(&asmrt);
		assemble_selfcall(fn);
		// finalize code generation and get function pointer to the generated function
		func_t foo = fn.finalize(&asmrt);
		// execute the generated function
		uint32_t result = foo(index);
		printf("result: %u; expected: %u\n", result, expected);
	}
#endif
#ifdef ENABLE_LLVMJIT
	{
		// context object representing the generated function
		coat::Function<coat::runtimellvmjit,func_t> fn(llvmrt);
		assemble_selfcall(fn);
		// finalize code generation and get function pointer to the generated function
		func_t foo = fn.finalize(llvmrt);
		// execute the generated function
		uint32_t result = foo(index);
		printf("result: %u; expected: %u\n", result, expected);
	}
#endif

#ifdef ENABLE_ASMJIT
	{
		coat::Function<coat::runtimeasmjit,func_t> fnrec(&asmrt);
		assemble_selfcall(fnrec);
		func_t foorec = fnrec.finalize(&asmrt);

		coat::Function<coat::runtimeasmjit,func_t> fn(&asmrt);
		assemble_crosscall(fn, foorec, "");
		func_t foo = fn.finalize(&asmrt);
		// execute the generated function
		uint32_t result = foo(index);
		printf("result: %u; expected: %u\n", result, expected);
	}
#endif
#ifdef ENABLE_LLVMJIT
	{
		coat::Function<coat::runtimellvmjit,func_t> fnrec(llvmrt, "rec");
		assemble_selfcall(fnrec);
		func_t foorec = fnrec.finalize(llvmrt);

		coat::Function<coat::runtimellvmjit,func_t> fn(llvmrt, "caller");
		assemble_crosscall(fn, foorec, "rec");
		func_t foo = fn.finalize(llvmrt);
		// execute the generated function
		uint32_t result = foo(index);
		printf("result: %u; expected: %u\n", result, expected);
	}
#endif

	return 0;
}
