#include <cstdlib>
#include <cstdio>

#include "include/Function.h"
#include "include/ControlFlow.h"



// generate code, in this case just sum all element in array
template<class Fn>
void assemble_sum_foreach(Fn &fn){
	auto args = fn.getArguments("arr", "cnt");
	auto &vr_arr = std::get<0>(args);
	auto &vr_cnt = std::get<1>(args);

	coat::Value vr_sum(fn, 0, "sum");
	auto vr_arrend = vr_arr + vr_cnt;
	coat::for_each(fn, vr_arr, vr_arrend, [&](auto &vr_ele){
		vr_sum += vr_ele;
	});
	coat::ret(fn, vr_sum);
}

// generate code, in this case just sum all element in array
template<class Fn>
void assemble_sum_counter(Fn &fn){
	auto args = fn.getArguments("arr", "cnt");
	auto &vr_arr = std::get<0>(args);
	auto &vr_cnt = std::get<1>(args);

	coat::Value vr_sum(fn, 0, "sum");
	coat::Value vr_idx(fn, 0UL, "idx");
	coat::loop_while(fn, vr_idx < vr_cnt, [&](){
		vr_sum += vr_arr[vr_idx];
		++vr_idx;
	});
	coat::ret(fn, vr_sum);
}



int main(int argc, char **argv){
	if(argc < 3){
		puts("usage: ./prog divisor array_of_numbers\nexample: ./prog 2 24 32 86 42 23 16 4 8");
		return -1;
	}

	int divisor = atoi(argv[1]);
	size_t cnt = argc - 2;
	int *array = new int[cnt];
	for(int i=2; i<argc; ++i){
		array[i-2] = atoi(argv[i]);
	}


	// init JIT backends
	coat::runtimeAsmjit asmrt;
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;

	{
		using func_type = int (*)();
		coat::Function<coat::runtimeAsmjit,func_type> fn(&asmrt);
		coat::Value vr_val3(fn, 0, "val");
		coat::Value<::asmjit::x86::Compiler,int> vr_val(fn, "val");
		vr_val = 0;
		auto vr_val2 = fn.getValue<int>("val");
		vr_val2 = 0;
		coat::ret(fn, vr_val);

		// finalize function
		func_type fnptr = fn.finalize(&asmrt);
		// execute generated function
		int result = fnptr();
		printf("initialization test:  asmjit; result: %i\n", result);

		asmrt.rt.release(fnptr);
	}
	{
		using func_type = int (*)();
		coat::Function<coat::runtimellvmjit,func_type> fn(llvmrt);
		coat::Value vr_val3(fn, 0, "val");
		coat::Value<::llvm::IRBuilder<>,int> vr_val(fn, "val");
		vr_val = 0;
		auto vr_val2 = fn.getValue<int>("val");
		vr_val2 = 0;
		coat::ret(fn, vr_val);

		// finalize function
		func_type fnptr = fn.finalize(llvmrt);
		// execute generated function
		int result = fnptr();
		printf("initialization test: llvmjit; result: %i\n", result);
		//FIXME: free function
	}

	{
		using func_type = int (*)(const int*,size_t);
		coat::Function<coat::runtimeAsmjit,func_type> fn(&asmrt);
		assemble_sum_foreach(fn);

		// finalize function
		func_type fnptr = fn.finalize(&asmrt);
		// execute generated function
		int result = fnptr(array, cnt);
		printf("result with for_each and  asmjit: %i\n", result);

		asmrt.rt.release(fnptr);
	}
	{
		using func_type = int (*)(const int*,size_t);
		coat::Function<coat::runtimellvmjit,func_type> fn(llvmrt);
		assemble_sum_foreach(fn);

		// finalize function
		func_type fnptr = fn.finalize(llvmrt);
		// execute generated function
		int result = fnptr(array, cnt);
		printf("result with for_each and llvmjit: %i\n", result);
		//FIXME: free function
	}

	{
		using func_type = int (*)(const int*,size_t);
		coat::Function<coat::runtimeAsmjit,func_type> fn(&asmrt);
		assemble_sum_counter(fn);

		// finalize function
		func_type fnptr = fn.finalize(&asmrt);
		// execute generated function
		int result = fnptr(array, cnt);
		printf("result with loop_while and  asmjit: %i\n", result);

		asmrt.rt.release(fnptr);
	}
	{
		using func_type = int (*)(const int*,size_t);
		coat::Function<coat::runtimellvmjit,func_type> fn(llvmrt);
		assemble_sum_counter(fn);

		// finalize function
		func_type fnptr = fn.finalize(llvmrt);
		// execute generated function
		int result = fnptr(array, cnt);
		printf("result with loop_while and llvmjit: %i\n", result);
		//FIXME: free function
	}

	delete[] array;

	return 0;
}
