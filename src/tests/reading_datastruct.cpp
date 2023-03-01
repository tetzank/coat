#include <cstdio>
#include <vector>
#include <numeric>

#include <coat/Function.h>
#include <coat/ControlFlow.h>


class my_vector {
// annotate type with name of type for better debug info
COAT_NAME("my_vector");
// declare all member variables: int *start, *finish, *capacity;
// macro adds metadata to calculate
// data layout at compile-time
#define MEMBERS(x)    \
    x(int*, start)    \
    x(int*, finish)   \
    x(int*, capacity)

COAT_DECLARE_PRIVATE(MEMBERS)
#undef MEMBERS

public:
	my_vector(){
		// set it to some hardcoded state
		start = new int[42];
		finish = start + 23;
		capacity = start + 42;
	}
	~my_vector(){
		delete[] start;
	}

	size_t size() const {
		return finish - start;
	}
};



int main(){
	// signature of the generated function: taking pointer to data structure
	using func_t = size_t (*)(my_vector *vec);

#ifdef ENABLE_ASMJIT
	// initialize backend, AsmJit in this case
	coat::runtimeasmjit asmrt;
	// context object representing the generated function
	auto fn = asmrt.createFunction<func_t>("reading");
#elif defined(ENABLE_LLVMJIT)
	// initialize LLVM backend
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	// context object representing the generated function
	auto fn = llvmrt.createFunction<func_t>("reading");
#else
#	error "Neither AsmJit nor LLVM enabled"
#endif

	// start of the EDSL code describing the code of the generated function
	{
		// get function arguments as "meta-variables"
		auto [vec] = fn.getArguments("vec");

		// read member "start"
		auto start = vec.get_value<my_vector::member_start>("start");
		// read member "finish"
		auto finish = vec.get_value<my_vector::member_finish>("finish");
		// calculate number of elements between both pointers
		auto size = finish - start;

		// specify return value
		coat::ret(fn, size);
	}
	// finalize code generation and get function pointer to the generated function
	func_t foo = fn.finalize();

	my_vector vec;
	// execute the generated function
	size_t result = foo(&vec);

	// print result
	size_t expected = vec.size();
	if(result == expected){
		printf("correct result: %lu\n", result);
		return 0;
	}else{
		printf("wrong result:\nresult: %lu; expected: %lu\n", result, expected);
		return -1;
	}
}
