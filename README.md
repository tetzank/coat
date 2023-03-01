COAT: COdegen Abstract Types
===

COAT is an embedded domain specific language (EDSL) for C++ to make just-in-time (JIT) code generation easier.
It provides abstract types and control flow constructs to express the data flow you want to generate and execute at runtime.
All operations are transformed at compile-time to appropriate API calls of a supported JIT engine, currently AsmJit and LLVM.

Code specialization has a huge impact on performance.
Tailored code takes advantage of the knowledge about the involved data types and operations.
In C++, we can instruct the compiler to generate specialized code at compile-time with the help of template metaprogramming and constant expressions.
However, constant evaluation is limited as it cannot leverage runtime information.
JIT compilations lifts this limitation by enabling programs to generate code at runtime.
This library tries to make JIT compilation as easy as possible.

More details are explained in a [blog post](https://tetzank.github.io/posts/coat-edsl-for-codegen/).


## Build Instructions

Ensure submodules are fetched, and build with CMake:

```
git submodule init
git submodule update
mkdir build
cd build
cmake ..
make -j4
make test
```

The library is header-only. Small example programs are included to show how to use the library:

```
cd build
./example_coat_llvmjit_calculator
./example_coat_asmjit_calculator
```

## Example

The following code snippet is a full example program.
It generates a function which calculates the sum of all elements for a passed array.
This is hardly a good application of JIT compilation, but it shows how easy and readable code generation is with the help of COAT.

```C++
#include <cstdio>
#include <vector>
#include <numeric>

#include <coat/Function.h>
#include <coat/ControlFlow.h>


int main(){
	// generate some data
	std::vector<uint64_t> data(1024);
	std::iota(data.begin(), data.end(), 0);

	// initialize backend, AsmJit in this case
	coat::runtimeasmjit asmrt;
	// signature of the generated function
	using func_t = uint64_t (*)(uint64_t *data, uint64_t size);
	// context object representing the generated function
	auto fn = asmrt.createFunction<func_t>();
	// start of the EDSL code describing the code of the generated function
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
	func_t foo = fn.finalize();

	// execute the generated function
	uint64_t result = foo(data.data(), data.size());

	// print result
	uint64_t expected = std::accumulate(data.begin(), data.end(), 0);
	printf("result: %lu; expected: %lu\n", result, expected);

	return 0;
}
```

A more comprehensive example is provided in [another repository](https://github.com/tetzank/sigmod18contest), using COAT in the context of query processing in a relational database.


## Type-Based Compilation

COAT provides special types representing variables in the generated function, named "meta-variables" for easier discussion.
All operators on these types are overloaded to emit the corresponding instructions through the chosen JIT compiler backend.
This results in a two-staged compilation depending on the types used.
C++ variables and all operations on them get compiled at compile-time as normal.
Operations on "meta-variables" get transformed to JIT API calls at compile-time which generate machine code at runtime.

The type support is very limited at the moment.
The only fundamental type is `coat::Value` which represents a signed/unsigned integer of size 1, 2, 4 or 8 bytes.
Floating point types are currently not supported.
Furthermore, `coat::Ref` and `coat::Ptr` represent reference and pointer types to `coat::Value`.
Additionally, `coat::Struct` represents a pointer to an aggregate type like a struct or class, providing access to member variables.
It is discussed in more detail in the section "Host Program Integration" below.


## Control Flow Abstractions

Next to types for code generation, COAT provides helper functions simulating loop and branch constructs of C++.
The following listing relates these abstractions to their C++ counterpart.

```C++
coat::if_then(coat::Function &ctx, condition, [&]{      // if( condition ){
    then_branch                                         //     then_branch
});                                                     // }


coat::if_then_else(coat::Function &ctx, condition, [&]{ // if( condition ){
    then_branch                                         //     then_branch
}, [&]{                                                 // } else {
    else_branch                                         //     else_branch
});                                                     // }


coat::loop_while(coat::Function &ctx, condition, [&]{   // while( condition ){
    loop_body                                           //     loop_body
});                                                     // }


coat::do_while(coat::Function &ctx, [&]{                // do {
    loop_body                                           //     loop_body
}, condition);                                          // } while( condition );


coat::for_each(coat::Function &ctx, begin, end,         // for( ; begin != end; ++begin ){
    [&](auto &element){                                 //     auto &element = *begin;
        loop_body                                       //     loop_body
    }                                                   //
);                                                      // }
```

With the help of lambda expressions, we can format the code in similar way than C++ code, improving readability and maintainability.


## Explicit Vectorization

Limited vectorization support is provided by `coat::Vector`, supporting basic arithmetic and bitwise operations.
The following example shows how to use `coat::Vector` for explicit vectorization.
It calculates the mean value for each element in two input arrays and writes it to a destination array.

```C++
// number of elements in vector
static const int vectorsize = 4;
{ // EDSL
	// function parameters: 2 source arrays, destination array, size of arrays
	auto [aptr,bptr,rptr,sze] = fn.getArguments("a", "b", "r", "size");
	// index into arrays
	coat::Value pos(fn, uint64_t(0), "pos");

	// check if at least one vector of work
	coat::if_then(fn, sze > vectorsize, [&]{
		// vectorized loop
		coat::do_while(fn, [&]{
			// rptr[pos] = (make_vector<vectorsize>(fn, aptr[pos]) += bptr[pos]) /= 2;
			auto vec = coat::make_vector<vectorsize>(fn, aptr[pos]);
			vec += bptr[pos];
			vec /= 2;
			vec.store(rptr[pos]);
			// move to next vector
			pos += vec.getWidth();
		}, pos < sze);
		// reverse increase as it went over sze
		pos -= vectorsize;
	});
	// tail handling
	coat::loop_while(fn, pos < sze, [&]{
		auto a = fn.getValue<uint32_t>();
		a = aptr[pos];
		rptr[pos] = (a += bptr[pos]) /= 2;
		++pos;
	});
	coat::ret(fn);
}
```


## Host Program Integration

COAT provides multiple functionalities to make interaction between the generated code and the host program as seamless as possible.
This includes calling functions and accessing data structures of the host program.


### Calling Functions

Not every function benefits from JIT compilation.
COAT provides the helper function `FunctionCall()` generating a function call to the passed function pointer.
This way, you do not need to reimplement existing functions with COAT to make them usable for JIT compilation.
You just call them from the generated code with the correct calling convention of the system.

```C++
// function to call, residing outside of generated code
void log(int value);

...

coat::Value value(fn, 42, "value");
// generating the call in COAT
// parameter: context object, function pointer, name, function parameters
coat::FunctionCall(fn, log, "log", value);
```

When the called function has a return value, it can be captured in a "meta-variable".
The list of parameters and the return type are inferred from the function pointer.

```C++
// function to call, residing outside of generated code
int hash(int value);

...

coat::Value value(fn, 42, "value");
// generating the call in COAT
// parameter: context object, function pointer, name, function parameters
auto result = coat::FunctionCall(fn, hash, "hash", value);
```


### Reading from Data Structures

Data structures of the host program may be accessed directly from within the generated code.
The member variables of a data structure must be created with a special macro as shown below.
The macro adds a bit of metadata to the type, such that the data layout can be calculated.
C++ has no compile-time introspection yet.
The metadata does not leak to the runtime.
The size of the data structure stays unchanged.

```C++
class my_vector {
// declare all member variables: int *start, *finish, *capacity;
// macro adds metadata to calculate data layout at compile-time
#define MEMBERS(x)    \
    x(int*, start)    \
    x(int*, finish)   \
    x(int*, capacity)

COAT_DECLARE_PRIVATE(MEMBERS)
#undef MEMBERS

public:
    my_vector();
    ...
};
```

With the metadata in place, COAT creates wrapper types automatically for pointers to such a data structure.
The wrapper allows read and write access to all member variables.
In the following code snippet, we read and copy the value with `get_value<memberindex>` where `memberindex` is an enum entry which was created by the macros as well.

```C++
// function signature, taking pointer to my_vector and returning size
using func_t = size_t (*)(my_vector *vec);
// context object representing generated function
coat::Function<coat::runtimeasmjit,func_t> fn(asmrt);
{ // start of EDSL code describing function to generate
	// get function argument
	auto [vec] = fn.getArguments("vec");
	// read member "start"
	auto start = vec.get_value<my_vector::member_start>();
	// read member "finish"
	auto finish = vec.get_value<my_vector::member_finish>();
	// calculate number of elements between both pointers
	auto size = finish - start;
	coat::ret(fn, size);
}
// finalize code generation and get function pointer to generated code
func_t foo = fn.finalize();
```


### Writing into Data Structures

For writing, we get a reference instead of a copy by using `get_reference<memberindex>`.
Assigning values to the reference will generate code to write to the data structure.

```C++
// function signature, taking pointer to my_vector and a value to push on vector
using func_t = void (*)(my_vector *vec, int value);
// context object representing generated function
coat::Function<coat::runtimeasmjit,func_t> fn(asmrt);
{ // start of EDSL code describing function to generate
	// get function argument
	auto [vec,value] = fn.getArguments("vec", "value");
	// read member "finish"
	auto finish = vec.get_value<my_vector::member_finish>();
	// read member "capacity"
	auto capacity = vec.get_value<my_vector::member_capacity>();
	// grow if full
	coat::if_then(fn, finish == capacity, [&]{
		// call function vector_grow(my_vector*) to grow allocated memory
		coat::FunctionCall(fn, vector_grow, "vector_grow", vec);
		// re-read member "finish" as address of allocated memory could be different
		finish = vec.get_value<my_vector::member_finish>();
	});
	// add new value
	*finish = value;
	// move finish to new past-the-end position
	++finish;
	// write the changed past-the-end pointer back to the data structure
	vec.get_reference<PV::member_finish>() = vr_finish;
	coat::ret(fn);
}
// finalize code generation and get function pointer to generated code
func_t foo = fn.finalize();
```


## Profiling Instructions

Profiling is currently only supported for the AsmJit backend and perf on Linux.
Support has to be enabled by defining `PROFILING_ASSEMBLY` or `PROFILING_SOURCE`.
For the example programs, set the cmake variable `PROFILING` to either "ASSEMBLY" or "SOURCE".
"ASSEMBLY" instructs COAT to dump the generated code to a special file which enables `perf report` to annotate the generated instructions with profile information.
Hence, you can profile the generated code on the assembly level.
"SOURCE" adds debug information to map instructions to the C++ source using COAT which generated the instruction.
`perf report` will show the source line next to the instruction.

Profiling steps:
1. `perf record -k 1 ./application`
2. `perf inject -j -i perf.data -o perf.data.jitted`
3. `perf report -i perf.data.jitted`

The generated function should appear with the chosen name in the list of functions.
You can zoom into the function and annotate the instructions with profile information by pressing 'a'.


## Publications

Paper:<br>
Tetzel, F., Lehner, W. & Kasperovics, R. Efficient Compilation of Regular Path Queries. _Datenbank Spektrum_ 20, 243–259 (2020). https://doi.org/10.1007/s13222-020-00353-9

Blog posts:<br>
[COAT: EDSL for Codegen](https://tetzank.github.io/posts/coat-edsl-for-codegen/)<br>
[Codegen in Databases](https://tetzank.github.io/posts/codegen-in-databases/)


## Similar Projects

[CodeGen](https://github.com/pdziepak/codegen) - similar wrapper library on top of LLVM

[ClangJIT](https://github.com/hfinkel/llvm-project-cxxjit) - fork of Clang adding JIT compilation of annotated C++ templates

[Easy::Jit](https://github.com/jmmartinez/easy-just-in-time) - stores LLVM IR of annotated functions in executable for JIT compilation with LLVM at runtime
