#include <cstdio>
#include <vector>
#include <numeric>

#include <coat/Function.h>
#include <coat/ControlFlow.h>


int main(){
	// generate some data
	std::vector<uint64_t> data(1 << 25);
	std::iota(data.begin(), data.end(), 0);

	// signature of the generated function: taking pointer and size, returning sum
	using func_t = uint64_t (*)(uint64_t *data, uint64_t size);

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

#define _(var) var.setName(#var)
	// start of the EDSL code describing the code of the generated function
	{
		// get function arguments as "meta-variables"
		auto [data,size] = fn.getArguments("data", "size");

		// "meta-variable" for sum
		coat::Value sum(fn, uint64_t(0), "sum");
		// "meta-variable" for past-the-end pointer
		auto end = data + size; _(end);
		// loop over all elements
		coat::for_each(fn, data, end, [&](auto &element){
			// add each element to the sum
			sum += element;
		});
		// specify return value
		coat::ret(fn, sum);
	}
#ifdef ENABLE_LLVMJIT
	fn.printIR("sumLLVM.ll");
	if(!fn.verify()){
		puts("verification failed. aborting.");
		exit(EXIT_FAILURE); //FIXME: better error handling
	}
#endif
	// finalize code generation and get function pointer to the generated function
	func_t foo = fn.finalize();

	// execute the generated function
	uint64_t result = foo(data.data(), data.size());

	// print result
	uint64_t expected = std::accumulate(data.begin(), data.end(), uint64_t(0));
	if(result == expected){
		printf("correct result: %lu\n", result);
		return 0;
	}else{
		printf("wrong result:\nresult: %lu; expected: %lu\n", result, expected);
		return -1;
	}
}
