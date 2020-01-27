#include <cstdio>
#include <cstdint>
#include <vector>
#include <numeric>

#include <signal.h> // for debugging

#include <coat/Function.h>
#include <coat/ControlFlow.h>
#include <coat/Vector.h>


int main(){
	// function signature
	using func_type = void (*)(const uint32_t*, const uint32_t*, uint32_t*, size_t);

	// init
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	llvmrt.setOptLevel(2);
	// context object
	coat::Function<coat::runtimellvmjit,func_type> fn(llvmrt, "shuffle_llvmjit");
	{ // EDSL
		// function parameters: 2 source arrays, destination array, size of arrays
		auto [aptr,bptr,rptr,size] = fn.getArguments("a", "b", "r", "size");

#if 0
		// index into arrays
		coat::Value pos(fn, uint64_t(0), "pos");

		auto va = coat::make_vector<4>(fn, aptr[pos]);
		auto vb = coat::make_vector<4>(fn, bptr[pos]);

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

		vmin.store(rptr[pos]);
		pos += 4;
		vmax.store(rptr[pos]);

		//testing
		coat::Value limit(fn, uint64_t(1024), "pos");
		pos += coat::make_value<uint64_t>(fn, pos <= limit) * 4;

#else

		coat::Value index_a(fn, uint64_t(0), "index_a");
		coat::Value index_b(fn, uint64_t(0), "index_b");
		coat::Value count(fn, uint64_t(0), "count");



		auto va = coat::make_vector<4>(fn, aptr[index_a]);
		auto vb = coat::make_vector<4>(fn, bptr[index_b]);

		coat::Value max_a = aptr[index_a + 3];
		coat::Value max_b = bptr[index_b + 3];
		index_a += coat::make_value<uint64_t>(fn, max_a <= max_b) * 4;
		index_b += coat::make_value<uint64_t>(fn, max_a >= max_b) * 4;

		auto vrot1 = coat::shuffle(fn, vb, {3, 0, 1, 2}); // rotating right
		auto vrot2 = coat::shuffle(fn, vb, {1, 2, 3, 0}); // rotating left
		auto vrot3 = coat::shuffle(fn, vb, {2, 3, 0, 1}); // between

		auto mask = ((va == vb) |= (va == vrot1)) |= ((va == vrot2) |= (va == vrot3));

		//count += va.compressstore(rptr[count], mask);
		va.compressstore(rptr[count], mask);
		count += mask.movemask().popcount();

#endif

		coat::ret(fn);
	}

	llvmrt.print("shuffle.ll");
	if(!llvmrt.verifyFunctions()){
		puts("verification failed. aborting.");
		exit(EXIT_FAILURE);
	}
	llvmrt.optimize();
	llvmrt.print("shuffle_opt.ll");
	if(!llvmrt.verifyFunctions()){
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
