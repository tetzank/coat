#include <cstdio>
#include <cstdint>
#include <vector>
#include <numeric>

#include <asmjit/asmjit.h>

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


void mean_asmjit(
	const uint32_t * __restrict__ a,
	const uint32_t * __restrict__ b,
	      uint32_t * __restrict__ r,
	size_t size
){

	// init asmjit
	asmjit::JitRuntime rt;

	asmjit::CodeHolder code;
	code.init(rt.codeInfo());
	asmjit::FileLogger logger(stdout); // write to stdout
	code.setLogger(&logger);
	asmjit::x86::Compiler cc(&code);

	cc.addFunc(asmjit::FuncSignatureT<void,const uint32_t*,const uint32_t*, uint32_t*, size_t>());
	asmjit::x86::Gp r_a = cc.newUIntPtr("a");
	asmjit::x86::Gp r_b = cc.newUIntPtr("b");
	asmjit::x86::Gp r_r = cc.newUIntPtr("r");
	asmjit::x86::Gp r_size = cc.newUInt64("size");
	cc.setArg(0, r_a);
	cc.setArg(1, r_b);
	cc.setArg(2, r_r);
	cc.setArg(3, r_size);

	asmjit::x86::Gp r_pos = cc.newUInt64("pos");
	cc.xor_(r_pos, r_pos);
	asmjit::Label l_loop = cc.newLabel();

#if 0
	asmjit::x86::Ymm r_vec = cc.newYmm("vec");

	cc.bind(l_loop);
	cc.vmovdqu(r_vec, asmjit::x86::ymmword_ptr(r_a, r_pos));
	cc.vpaddd(r_vec, r_vec, asmjit::x86::ymmword_ptr(r_b, r_pos));
	cc.vpsrld(r_vec, r_vec, 1);
	cc.vmovdqu(asmjit::x86::ymmword_ptr(r_r, r_pos), r_vec);
	cc.add(r_pos, 32);
	cc.cmp(r_pos, r_size); //FIXME: only works when there's no tail to handle
	cc.jne(l_loop);
#else
	asmjit::x86::Xmm r_vec = cc.newXmm("vec");

	cc.bind(l_loop);
	cc.movdqu(r_vec, asmjit::x86::xmmword_ptr(r_a, r_pos, 2));
	cc.paddd(r_vec, asmjit::x86::xmmword_ptr(r_b, r_pos, 2));
	cc.psrld(r_vec, 1);
	cc.movdqu(asmjit::x86::xmmword_ptr(r_r, r_pos, 2), r_vec);
	cc.add(r_pos, 4);
	cc.cmp(r_pos, r_size); //FIXME: only works when there's no tail to handle
	cc.jne(l_loop);
#endif

	cc.endFunc();
	cc.finalize();

	func_type fn;
	asmjit::Error err = rt.add(&fn, &code);
	if(err){
		fprintf(stderr, "runtime add failed with CodeCompiler\n");
		std::exit(1);
	}

	// execute
	fn(a, b, r, size);

	rt.release(fn);
}

void mean_coat_asmjit(
	const uint32_t * __restrict__ a,
	const uint32_t * __restrict__ b,
	      uint32_t * __restrict__ r,
	size_t size
){
	// init
	coat::runtimeasmjit asmrt;
	// context object
	coat::Function<coat::runtimeasmjit,func_type> fn(asmrt);

	{
		auto [aptr,bptr,rptr,sze] = fn.getArguments("a", "b", "r", "size");
		coat::Value pos(fn, uint64_t(0), "pos");
		
		coat::Vector<asmjit::x86::Compiler,uint32_t,8> avec(fn), bvec(fn);
		coat::do_while(fn, [&]{
			avec = aptr[pos];
			bvec = bptr[pos];
			avec += bvec;
			avec >>= 1;
			avec.store(rptr[pos]);
			pos += avec.getWidth();
		}, pos != sze);
		coat::ret(fn);
	}
	func_type foo = fn.finalize();

	// execute
	foo(a, b, r, size);
}

void mean_coat_llvmjit(
	const uint32_t * __restrict__ a,
	const uint32_t * __restrict__ b,
	      uint32_t * __restrict__ r,
	size_t size
){
	// init
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	// context object
	coat::Function<coat::runtimellvmjit,func_type> fn(llvmrt);

	{
		auto [aptr,bptr,rptr,sze] = fn.getArguments("a", "b", "r", "size");
		coat::Value pos(fn, uint64_t(0), "pos");
		
		coat::Vector<::llvm::IRBuilder<>,uint32_t,8> avec(fn), bvec(fn);
		coat::do_while(fn, [&]{
			avec = aptr[pos];
			bvec = bptr[pos];
			avec += bvec;
			avec >>= 1;
			avec.store(rptr[pos]);
			pos += avec.getWidth();
		}, pos != sze);
		coat::ret(fn);
	}
	func_type foo = fn.finalize();

	// execute
	foo(a, b, r, size);
}



static void print(const std::vector<uint32_t> &vec){
	for(size_t i=0, s=vec.size(); i<s; ++i){
		printf("%u, ", vec[i]);
	}
}

int main(){
	// generate some data
	std::vector<uint32_t> a, b, r, e;
	a.resize(1024);
	std::iota(a.begin(), a.end(), 0);
	b.resize(1024);
	std::iota(b.begin(), b.end(), 2);
	r.resize(1024);
	e.resize(1024);
	std::iota(e.begin(), e.end(), 1);

	// call
	mean(a.data(), b.data(), r.data(), r.size());
	if(r == e){
		puts("mean: Success");
	}else{
		puts("mean: Failure");
		print(r);
	}

	// asmjit
	mean_asmjit(a.data(), b.data(), r.data(), r.size());
	if(r == e){
		puts("mean_asmjit: Success");
	}else{
		puts("mean_asmjit: Failure");
		print(r);
	}

	// coat
	mean_coat_asmjit(a.data(), b.data(), r.data(), r.size());
	if(r == e){
		puts("mean_coat_asmjit: Success");
	}else{
		puts("mean_coat_asmjit: Failure");
		print(r);
	}

	mean_coat_llvmjit(a.data(), b.data(), r.data(), r.size());
	if(r == e){
		puts("mean_coat_llvmjit: Success");
	}else{
		puts("mean_coat_llvmjit: Failure");
		print(r);
	}

	return 0;
}
