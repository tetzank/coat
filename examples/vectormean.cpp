#include <cstdio>
#include <cstdint>
#include <vector>
#include <numeric>

//#include <signal.h> // for debugging

#include <coat/Function.h>
#include <coat/ControlFlow.h>
#include <coat/Vector.h>


// function signature
using func_type = void (*)(const uint32_t*, const uint32_t*, uint32_t*, size_t);

void mean(
	const uint32_t * __restrict__ a,
	const uint32_t * __restrict__ b,
	      uint32_t * __restrict__ r,
	size_t size
){
	for(size_t i=0; i<size; ++i){
		r[i] = (a[i] + b[i]) / 2;
	}
}


void mean_coat(
	const uint32_t * __restrict__ a,
	const uint32_t * __restrict__ b,
	      uint32_t * __restrict__ r,
	size_t size
){
#ifdef ENABLE_ASMJIT
	// init
	coat::runtimeasmjit asmrt;
	// context object
	auto fn = asmrt.createFunction<func_type>("gen_asmjit");
	fn.enableCodeDump();
#elif defined(ENABLE_LLVMJIT)
	// init
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	// context object
	auto fn = llvmrt.createFunction<func_type>("gen_llvmjit");
#else
#	error "Neither AsmJit nor LLVM enabled"
#endif

	// number of elements in vector
	static const int vectorsize = 4;
	{ // EDSL
		// function parameters: 2 source arrays, destination array, size of arrays
		//FIXME: does not work in clang, bindings cannot be captured in lambda, do it the old fashioned way
		//auto [aptr,bptr,rptr,sze] = fn.getArguments("a", "b", "r", "size");
		auto args = fn.getArguments("a", "b", "r", "size");
		auto &aptr = std::get<0>(args);
		auto &bptr = std::get<1>(args);
		auto &rptr = std::get<2>(args);
		auto &sze = std::get<3>(args);
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
			//coat::Value a = aptr[pos]; //FIXME: does not work with source line profiling
			auto a = fn.getValue<uint32_t>();
			a = aptr[pos];
			rptr[pos] = (a += bptr[pos]) /= 2;
			++pos;
		});
		coat::ret(fn);
	}

#ifdef ENABLE_LLVMJIT
	if(!fn.verify()){
		puts("verification failed. aborting.");
		fn.printIR("failed.ll");
		exit(EXIT_FAILURE);
	}
	fn.optimize(2);
	if(!fn.verify()){
		puts("verification after optimization failed. aborting.");
		fn.printIR("failed_opt.ll");
		exit(EXIT_FAILURE);
	}
	//TODO
	//llvmrt.dumpAssembly("llvmjit_mean.s");
#endif
	func_type foo = fn.finalize();

	// execute
	//raise(SIGTRAP); // stop debugger here
	foo(a, b, r, size);
}


static void print(const std::vector<uint32_t> &vec){
	for(size_t i=0, s=vec.size(); i<s; ++i){
		printf("%u, ", vec[i]);
	}
	printf("\n");
}

int main(){
	// generate some data
	std::vector<uint32_t> a, b, r, e;
	static const int datasize = 4*1024*1024 + 3;
	a.resize(datasize);
	std::iota(a.begin(), a.end(), 0);
	b.resize(datasize);
	std::iota(b.begin(), b.end(), 2);
	r.resize(datasize);
	e.resize(datasize);
	std::iota(e.begin(), e.end(), 1);

	// call
	mean(a.data(), b.data(), r.data(), r.size());
	if(r == e){
		puts("mean: Success");
	}else{
		puts("mean: Failure");
		print(r);
	}

	// coat
	mean_coat(a.data(), b.data(), r.data(), r.size());
	if(r == e){
		puts("mean_coat: Success");
		return 0;
	}else{
		puts("mean_coat: Failure");
		print(r);
		return -1;
	}
}
