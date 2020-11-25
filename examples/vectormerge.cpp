#include <cstdio>
#include <cstdint>
#include <vector>
#include <numeric>

#include <signal.h> // for debugging

#include <coat/Function.h>
#include <coat/ControlFlow.h>
#include <coat/Vector.h>

//TODO: finish implementation to be a real example program

int main(){
	// function signature
	using func_type = void (*)(const uint32_t*, const uint32_t*, uint32_t*, size_t);

	// init
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	// context object
	auto fn = llvmrt.createFunction<func_type>("merge_llvmjit");
	{ // EDSL
		// function parameters: 2 source arrays, destination array, size of arrays
		auto [aptr,bptr,rptr,size] = fn.getArguments("a", "b", "r", "size");

		// index into arrays
		coat::Value pos(fn, uint64_t(0), "pos");

		auto va = coat::make_vector<4>(fn, aptr[0]);
		auto vb = coat::make_vector<4>(fn, bptr[0]);

		auto vmax = coat::max(fn, va, vb);
		auto vmin = coat::min(fn, va, vb);

		vb = coat::shuffle(fn, vmax, {3, 0, 1, 2}); // rotating right
		vmax = coat::max(fn, vmin, vb);
		vmin = coat::min(fn, vmin, vb);

		vb = coat::shuffle(fn, vmax, {3, 0, 1, 2}); // rotating right
		vmax = coat::max(fn, vmin, vb);
		vmin = coat::min(fn, vmin, vb);

		vb = coat::shuffle(fn, vmax, {3, 0, 1, 2}); // rotating right
		vmax = coat::max(fn, vmin, vb);
		vmin = coat::min(fn, vmin, vb);

		// store vector of smallest values
		vmin.store(rptr[pos]);
		pos += 4;

		coat::ret(fn);
	}

	fn.printIR("merge.ll");
	if(!fn.verify()){
		puts("verification failed. aborting.");
		exit(EXIT_FAILURE);
	}
	fn.optimize(2);
	fn.printIR("merge_opt.ll");
	if(!fn.verify()){
		puts("verification after optimization failed. aborting.");
		exit(EXIT_FAILURE);
	}

	func_type foo = fn.finalize();


	// generate some data
	std::vector<uint32_t> inputa, inputb, result;
	static const int datasize = 16;
	inputa.resize(datasize);
	std::iota(inputa.begin(), inputa.end(), 0);
	inputb.resize(datasize);
	std::iota(inputb.begin(), inputb.end(), 0);
	result.resize(datasize * 2);

	raise(SIGTRAP); // stop debugger here
	foo(inputa.data(), inputb.data(), result.data(), result.size());

	for(size_t i=0; i<datasize; ++i){
		printf("%u, ", result[i]);
	}
	printf("\n");

	return 0;
}
