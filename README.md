COAT: COdegen Abstract Types
===

COAT is an embedded domain specific language (EDSL) for C++ to make just-in-time (JIT) code generation easier.
It provides abstract types and control flow constructs to express the data flow you want to generate and execute at runtime.
All operations are transformed at compile-time to appropriate API calls into a supported JIT engine, currently AsmJit and LLVM.

Code specialization has a huge impact on performance.
Tailored code takes advantage of the knowledge about the involved data types and operations.
In C++, we can instruct the compiler to generate specialized code at compile-time with the help of template metaprogramming and constant expressions.
However, constant evaluation is limited as it cannot leverage runtime information.
Just-in-time compilations lifts this limitation by enabling programs to generate code at runtime.


## Example

The following code snippet is a full example program.
It generates a function which calculates the sum of all elements for a passed array.
This is hardly a usefull application of JIT compilation, but it is small enough for a full example and still shows the benefits of COAT.

```C++
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
```


## Build Instructions

The library is header-only. Small test programs are included to show how to use the library.

Fetch JIT engines and build them:
```
$ ./buildDependencies.sh
```

Then, build with cmake:
```
$ mkdir build
$ cd build
$ cmake ..
$ make
```