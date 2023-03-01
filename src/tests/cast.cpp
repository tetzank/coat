#include <cstdio>
#include <vector>
#include <numeric>

#include <coat/Function.h>
#include <coat/ControlFlow.h>


int main(){
	// allocate buffer
	uint8_t *buffer = new uint8_t[sizeof(uint64_t)+sizeof(uint32_t)+sizeof(uint8_t)];

	// signature of the generated function: taking pointer and size, returning sum
	using func_t = void (*)(uint8_t *buffer);

#ifdef ENABLE_ASMJIT
	// initialize backend, AsmJit in this case
	coat::runtimeasmjit asmrt;
	// context object representing the generated function
	auto fn = asmrt.createFunction<func_t>();
#elif defined(ENABLE_LLVMJIT)
	// initialize LLVM backend
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	// context object representing the generated function
	auto fn = llvmrt.createFunction<func_t>();
#else
#	error "Neither AsmJit nor LLVM enabled"
#endif

	// start of the EDSL code describing the code of the generated function
	{
		// get function arguments as "meta-variables"
		auto [buf] = fn.getArguments("buf");

		auto first = coat::cast<uint64_t*>(buf);
		*first = 0xCAFEBABE'CAFED00Dul;
		buf += sizeof(uint64_t);
		auto second = coat::cast<uint32_t*>(buf);
		*second = 0xDEADC0DEu;
		buf += sizeof(uint32_t);
		*buf = 42;

		// specify return
		coat::ret(fn);
	}
	// finalize code generation and get function pointer to the generated function
	func_t foo = fn.finalize();

	// execute the generated function
	foo(buffer);

	// check result
	uint8_t *ptr = buffer;
	uint64_t first = *(uint64_t*)ptr;
	ptr += sizeof(uint64_t);
	uint32_t second = *(uint32_t*)ptr;
	ptr += sizeof(uint32_t);
	uint8_t third = *ptr;
	printf("first: %#lx\nsecond: %#x\nthird: %u\n", first, second, third);

	delete[] buffer;

	if(first==0xCAFEBABE'CAFED00Dul && second==0xDEADC0DEu && third==42){
		return 0;
	}else{
		puts("unexpected results");
		return -1;
	}
}
