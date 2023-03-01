#include <cstdlib>
#include <cstdio>
#include <vector>

#include "coat/Function.h"
#include "coat/ControlFlow.h"

#include "coat/datastructs/pod_vector.h"


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


// generate code, sum if element is odd
template<class Fn>
void assemble_condsum_loop(Fn &fn){
	auto args = fn.getArguments("arr", "cnt");
	auto &vr_arr = std::get<0>(args);
	auto &vr_cnt = std::get<1>(args);

	coat::Value vr_sum(fn, 0, "sum");
	auto vr_arrend = vr_arr + vr_cnt;
	coat::do_while(fn, [&]{
		auto vr_ele = *vr_arr;
		if_then(fn, (vr_ele & 1) != 0, [&]{
			vr_sum += vr_ele;
		});
		++vr_arr;
	}, vr_arr != vr_arrend);
	coat::ret(fn, vr_sum);
}



struct triple {
#define MEMBERS(x) \
	x(uint32_t, first) \
	x(uint16_t, second) \
	x(uint64_t, third)

COAT_DECLARE(MEMBERS)
#undef MEMBERS
};

template<class Fn>
void assemble_getStructElement(Fn &fn){
	auto args = fn.getArguments("triple");
	auto &vr_triple = std::get<0>(args);

	auto vr_m = vr_triple.template get_reference<triple::member_second>();
	auto vr_ret = fn.template getValue<uint32_t>();
	vr_ret.widen(vr_m);
	coat::ret(fn, vr_ret);
}

template<typename T>
struct wrapped_vector {
	static constexpr std::array member_names{"begin", "end", "capend"};
	using types = std::tuple<T*,T*,T*>;
};

template<class Fn>
void assemble_vectorsum(Fn &fn){
	auto args = fn.getArguments("vector");
	auto &vr_vector = std::get<0>(args);

	coat::Value vr_sum(fn, 0, "sum");
	auto vr_pos = vr_vector.template get_value<0>();
	auto vr_end = vr_vector.template get_value<1>();
	coat::for_each(fn, vr_pos, vr_end, [&](auto &vr_ele){
		vr_sum += vr_ele;
	});
	coat::ret(fn, vr_sum);
}


int main(int argc, char **argv){
	if(argc < 3){
		puts("usage: ./prog divisor array_of_numbers\nexample: ./prog 2 24 32 86 42 23 16 4 8");
		return -1;
	}

	//int divisor = atoi(argv[1]); //FIXME: remove, also from program arguments
	size_t cnt = argc - 2;
	int *array = new int[cnt];
	for(int i=2; i<argc; ++i){
		array[i-2] = atoi(argv[i]);
	}


	// init JIT backends
#ifdef ENABLE_ASMJIT
	coat::runtimeasmjit asmrt;
#endif
#ifdef ENABLE_LLVMJIT
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
#endif

#ifdef ENABLE_ASMJIT
	{
		using func_type = int (*)();
		// old way
		//coat::Function<coat::runtimeasmjit,func_type> fn(asmrt, "value_creation");
		// using factory, preferred way
		auto fn = asmrt.createFunction<func_type>("value_creation");
		// calling coat::Value ctor
		coat::Value<::asmjit::x86::Compiler,int> vr_val(fn, "val");
		vr_val = 0;
		// calling factory to get coat::Value of certain type
		auto vr_val2 = fn.getValue<int>("val");
		vr_val2 = 0;
		// calling ctor with initial value, infers type from initial value
		coat::Value vr_val3(fn, 0, "val");
		// return something
		coat::ret(fn, vr_val);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr();
		printf("initialization test:  asmjit; result: %i\n", result);

		asmrt.rt.release(fnptr);
	}
#endif
#ifdef ENABLE_LLVMJIT
	{
		using func_type = int (*)();
		auto fn = llvmrt.createFunction<func_type>("value_creation");
		// calling coat::Value ctor
		coat::Value<coat::LLVMBuilders,int> vr_val(fn, "val");
		vr_val = 0;
		// calling factory to get coat::Value of certain type
		auto vr_val2 = fn.getValue<int>("val");
		vr_val2 = 0;
		// calling ctor with initial value, infers type from initial value
		coat::Value vr_val3(fn, 0, "val");
		// return something
		coat::ret(fn, vr_val);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr();
		printf("initialization test: llvmjit; result: %i\n", result);
		//FIXME: free function
	}
#endif

#ifdef ENABLE_ASMJIT
	{
		using func_type = int (*)(const int*,size_t);
		auto fn = asmrt.createFunction<func_type>("sum_foreach");
		assemble_sum_foreach(fn);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr(array, cnt);
		printf("result with for_each and  asmjit: %i\n", result);

		asmrt.rt.release(fnptr);
	}
#endif
#ifdef ENABLE_LLVMJIT
	{
		using func_type = int (*)(const int*,size_t);
		auto fn = llvmrt.createFunction<func_type>("sum_foreach");
		assemble_sum_foreach(fn);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr(array, cnt);
		printf("result with for_each and llvmjit: %i\n", result);
		//FIXME: free function
	}
#endif

#ifdef ENABLE_ASMJIT
	{
		using func_type = int (*)(const int*,size_t);
		auto fn = asmrt.createFunction<func_type>("sum_counter");
		assemble_sum_counter(fn);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr(array, cnt);
		printf("result with loop_while and  asmjit: %i\n", result);

		asmrt.rt.release(fnptr);
	}
#endif
#ifdef ENABLE_LLVMJIT
	{
		using func_type = int (*)(const int*,size_t);
		auto fn = llvmrt.createFunction<func_type>("sum_counter");
		assemble_sum_counter(fn);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr(array, cnt);
		printf("result with loop_while and llvmjit: %i\n", result);
		//FIXME: free function
	}
#endif

#ifdef ENABLE_ASMJIT
	{
		using func_type = int (*)(const int*,size_t);
		auto fn = asmrt.createFunction<func_type>("condsum_loop");
		assemble_condsum_loop(fn);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr(array, cnt);
		printf("result with loop_while and  asmjit: %i\n", result);

		asmrt.rt.release(fnptr);
	}
#endif
#ifdef ENABLE_LLVMJIT
	{
		using func_type = int (*)(const int*,size_t);
		auto fn = llvmrt.createFunction<func_type>("condsum_loop");
		assemble_condsum_loop(fn);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr(array, cnt);
		printf("result with loop_while and llvmjit: %i\n", result);
		//FIXME: free function
	}
#endif

	triple t{23, 42, 88};
#ifdef ENABLE_ASMJIT
	{
		using func_type = uint32_t (*)(triple*);
		auto fn = asmrt.createFunction<func_type>("getStructElement");
		assemble_getStructElement(fn);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr(&t);
		printf("getStructElement  asmjit: %i\n", result);

		asmrt.rt.release(fnptr);
	}
#endif
#ifdef ENABLE_LLVMJIT
	{
		using func_type = uint32_t (*)(triple*);
		auto fn = llvmrt.createFunction<func_type>("getStructElement");
		assemble_getStructElement(fn);

		fn.printIR("test-triple-dump.ll");

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr(&t);
		printf("getStructElement llvmjit: %i\n", result);
		//FIXME: free function
	}
#endif

	std::vector<int> vec;
	vec.reserve(cnt);
	for(size_t i=0; i<cnt; ++i){
		vec.push_back(array[i]);
	}
#ifdef ENABLE_ASMJIT
	{
		using func_type = int (*)(wrapped_vector<int>*);
		auto fn = asmrt.createFunction<func_type>("vectorsum");
		assemble_vectorsum(fn);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr((wrapped_vector<int>*)&vec);
		printf("vectorsum  asmjit: %i\n", result);

		asmrt.rt.release(fnptr);
	}
#endif
#ifdef ENABLE_LLVMJIT
	{
		using func_type = int (*)(wrapped_vector<int>*);
		auto fn = llvmrt.createFunction<func_type>("vectorsum");
		assemble_vectorsum(fn);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		int result = fnptr((wrapped_vector<int>*)&vec);
		printf("vectorsum llvmjit: %i\n", result);
		//FIXME: free function
	}
#endif

	// testing asmjit backend
	pod_vector<int> pod_vec;
	for(size_t i=0; i<cnt; ++i){
		pod_vec.push_back(array[i]);
	}
#ifdef ENABLE_ASMJIT
	{
		using func_type = size_t (*)(pod_vector<int>*);
		auto fn = asmrt.createFunction<func_type>("sum_podvec");
		auto args = fn.getArguments("podvec");
		auto &vr_podvec = std::get<0>(args);
		auto vr_size = vr_podvec.size();

		coat::Value vr_sum(fn, 0, "sum");
		coat::for_each(fn, vr_podvec, [&](auto &vr_ele){
			vr_sum += vr_ele;
		});
		vr_podvec.push_back(vr_sum);

		coat::ret(fn, vr_size);

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		size_t result = fnptr(&pod_vec);
		printf("podvec  asmjit: %lu, last element: %i\n", result, pod_vec.back());

		asmrt.rt.release(fnptr);

		printf("current elements: ");
		for(const auto &ele : pod_vec){
			printf("%i, ", ele);
		}
		printf("\n");
	}
#endif

	// testing llvm backend
	pod_vec.resize(cnt);
#ifdef ENABLE_LLVMJIT
	{
		using func_type = size_t (*)(pod_vector<int>*);
		auto fn = llvmrt.createFunction<func_type>("sum_podvec");
		auto args = fn.getArguments("podvec");
		auto &vr_podvec = std::get<0>(args);
		auto vr_size = vr_podvec.size();

		coat::Value vr_sum(fn, 0, "sum");
		coat::for_each(fn, vr_podvec, [&](auto &vr_ele){
			vr_sum += vr_ele;
		});
		vr_podvec.push_back(vr_sum);

		coat::ret(fn, vr_size);


		fn.printIR("test-podvec-dump.ll");

		// finalize function
		func_type fnptr = fn.finalize();
		// execute generated function
		size_t result = fnptr(&pod_vec);
		printf("podvec  asmjit: %lu, last element: %i\n", result, pod_vec.back());
		//FIXME: free function

		printf("current elements: ");
		for(const auto &ele : pod_vec){
			printf("%i, ", ele);
		}
		printf("\n");
	}
#endif

	delete[] array;

	return 0;
}
