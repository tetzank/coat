#include <cstdio>
#include <fstream>
#include <vector>
#include <chrono>

#include "coat/Function.h"
#include "coat/ControlFlow.h"

#include "coat/datastructs/ArrayTable.h"


struct Column{
	uint64_t min;
	uint64_t max;
	std::vector<uint64_t> data;
};

Column read_column_file(const char *fname){
	Column column;
	std::ifstream ifs(fname);
	if(!ifs.is_open()){
		fprintf(stderr, "could not open %s\n", fname);
		exit(-1);
	}
	uint64_t min = std::numeric_limits<uint64_t>::max();
	uint64_t max = 0;
	for(uint64_t i; ifs >> i; ){
		if(i < min) min = i;
		if(i > max) max = i;
		column.data.push_back(i);
	}
	column.min = min;
	column.max = max;
	return column;
}

template<class HT>
[[gnu::noinline]] void probe_hardcoded(const HT &ht, const Column &col){
	auto start = std::chrono::high_resolution_clock::now();
	uint64_t res = 0;

	for(uint64_t v : col.data){
		auto it = ht.lookup(v);
		if(it == ht.end()) continue;

		res += v;
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	printf("probe phase: %lu us\nres: %lu\n", time, res);
}


#if defined(ENABLE_ASMJIT) || defined(ENABLE_LLVMJIT)
template<class Fn>
void assemble(Fn &fn){
	auto args = fn.getArguments("col", "size", "ht");
	auto &vr_col = std::get<0>(args);
	auto &vr_cnt = std::get<1>(args);
	auto &vr_ht = std::get<2>(args);

	coat::Value vr_res(fn, 0UL, "res");
	auto vr_end = vr_col + vr_cnt;
	coat::for_each(fn, vr_col, vr_end, [&](auto &vr_ele){
		auto vr_key = fn.template getValue<uint64_t>("key");
		vr_key = vr_ele;
		//coat::Value vr_key(fn, vr_ele, "key");
		vr_ht.lookup(vr_key, [&](auto &vr_val){
			//vr_res += vr_val;
			vr_res += vr_key;
		});
	});
	coat::ret(fn, vr_res);
}
#endif

#ifdef ENABLE_ASMJIT
template<class HT>
[[gnu::noinline]] void probe_asmjit(const HT &ht, const Column &col){
	auto start = std::chrono::high_resolution_clock::now();

	using func_type = uint64_t (*)(const uint64_t *col, size_t size, const HT *ht);
	// init backend
	coat::runtimeasmjit asmrt;
	
	coat::Function<coat::runtimeasmjit,func_type> fn(&asmrt);
	assemble(fn);
	// finalize function
	func_type fnptr = fn.finalize(&asmrt);
	// execute generated function
	size_t res = fnptr(col.data.data(), col.data.size(), &ht);

	asmrt.rt.release(fnptr);

	auto end = std::chrono::high_resolution_clock::now();
	auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	printf("probe phase: %lu us\nres: %lu\n", time, res);
}
#endif

#ifdef ENABLE_LLVMJIT
template<class HT>
[[gnu::noinline]] void probe_llvmjit(const HT &ht, const Column &col){
	auto start = std::chrono::high_resolution_clock::now();

	using func_type = uint64_t (*)(const uint64_t *col, size_t size, const HT *ht);
	// init backend
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	
	coat::Function<coat::runtimellvmjit,func_type> fn(llvmrt);
	assemble(fn);
	// finalize function
	func_type fnptr = fn.finalize(llvmrt);
	// execute generated function
	size_t res = fnptr(col.data.data(), col.data.size(), &ht);
	//FIXME: free function

	auto end = std::chrono::high_resolution_clock::now();
	auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	printf("probe phase: %lu us\nres: %lu\n", time, res);
}
#endif


int main(int argc, char *argv[]){
	if(argc != 3){
		printf("usage: %s probe_file build_file\n", argv[0]);
		return -1;
	}

	Column col_probe = read_column_file(argv[1]);
	Column col_build = read_column_file(argv[2]);
	printf("probe size: %lu, min: %lu; max: %lu\nbuild size: %lu; min: %lu; max: %lu\n",
		col_probe.data.size(), col_probe.min, col_probe.max,
		col_build.data.size(), col_build.min, col_build.max);

	// build phase
	auto start = std::chrono::high_resolution_clock::now();
	ArrayTable<uint64_t> ht(col_build.min, col_build.max);
	// if column is not unique, it will overwrite and only the last entry is stored
	uint64_t index=0;
	for(uint64_t v : col_build.data){
		ht.insert(v, index);
		++index;
	}
	auto end = std::chrono::high_resolution_clock::now();
	auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	printf("build phase: %lu us\n", time);

	puts("hardcoded C++");
	probe_hardcoded(ht, col_probe);

#ifdef ENABLE_ASMJIT
	puts("asmjit");
	probe_asmjit(ht, col_probe);
#endif
#ifdef ENABLE_LLVMJIT
	puts("llvmjit");
	probe_llvmjit(ht, col_probe);
#endif

	return 0;
}
