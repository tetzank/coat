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
	auto fn = llvmrt.createFunction<func_type>("intersection_llvmjit");
	{ // EDSL
		// function parameters: 2 source arrays, destination array, size of arrays
		auto [aptr,bptr,rptr,size] = fn.getArguments("a", "b", "r", "size");

		coat::Value index_a(fn, uint64_t(0), "index_a");
		coat::Value index_b(fn, uint64_t(0), "index_b");
		coat::Value count(fn, uint64_t(0), "count");



		auto va = coat::make_vector<4>(fn, aptr[index_a]);
		auto vb = coat::make_vector<4>(fn, bptr[index_b]);

		// look at last element in current vector
		coat::Value max_a = aptr[index_a + 3];
		coat::Value max_b = bptr[index_b + 3];
		// move to next vector for array with smaller values
		index_a += coat::make_value<uint64_t>(fn, max_a <= max_b) * 4;
		index_b += coat::make_value<uint64_t>(fn, max_a >= max_b) * 4;

		// shuffle one vector
		auto vrot1 = coat::shuffle(fn, vb, {3, 0, 1, 2}); // rotating right
		auto vrot2 = coat::shuffle(fn, vb, {1, 2, 3, 0}); // rotating left
		auto vrot3 = coat::shuffle(fn, vb, {2, 3, 0, 1}); // between
		// compute mask of common elements
		auto mask = ((va == vb) |= (va == vrot1)) |= ((va == vrot2) |= (va == vrot3));

		// store common elements contiguous in result array
		va.compressstore(rptr[count], mask);
		// move index for resulting array forward by number of stored elements
		count += mask.movemask().popcount();

		coat::ret(fn);
	}

	fn.printIR("intersection.ll");
	if(!fn.verify()){
		puts("verification failed. aborting.");
		exit(EXIT_FAILURE);
	}
	fn.optimize(2);
	fn.printIR("intersection_opt.ll");
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
