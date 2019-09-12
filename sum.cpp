#include <cstdio>
#include <vector>
#include <numeric>

#include "coat/Function.h"
#include "coat/ControlFlow.h"


int main(){
	// generate some data
	std::vector<uint64_t> data(1024);
	std::iota(data.begin(), data.end(), 0);

	// initialize backend, AsmJit in this case
	coat::runtimeasmjit asmrt;
	// signature of the generated function
	using func_t = uint64_t (*)(uint64_t *data, uint64_t size);
	// context object representing the generated function
	coat::Function<coat::runtimeasmjit,func_t> fn(&asmrt);
	// start of the EDSL describing the code of the generated function
	{
		// get function arguments as "meta-variables"
		auto [data,size] = fn.getArguments("data", "size");

		// "meta-variable" for sum
		coat::Value sum(fn, uint64_t(0), "sum");
		// "meta-variable" for past-the-end pointer
		auto end = data + size;
		// loop over all elements
		coat::for_each(fn, data, end, [&](auto &element){
			// add each element to the sum
			sum += element;
		});
		// specify return value
		coat::ret(fn, sum);
	}
	// finalize code generation and get function pointer to the generated function
	func_t foo = fn.finalize(&asmrt);

	// execute the generate function
	uint64_t result = foo(data.data(), data.size());

	// print result
	uint64_t expected = std::accumulate(data.begin(), data.end(), 0);
	printf("result: %lu; expected: %lu\n", result, expected);

	return 0;
}
