#include <cstdio>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>

#include <unistd.h> // for getopt

#include "coat/Function.h"
#include "coat/ControlFlow.h"


struct Table {
#define MEMBERS(x) \
	x(size_t, ncols) \
	x(size_t, nrows) \
	x(uint64_t*, data)

DECLARE(MEMBERS)
#undef MEMBERS

	Table(size_t ncols, size_t nrows) : ncols(ncols), nrows(nrows) {
		data = new uint64_t[ncols * nrows];
	}
	~Table(){
		delete[] data;
	}

	      uint64_t *operator[](size_t col)       { return data + (col * nrows); }
	const uint64_t *operator[](size_t col) const { return data + (col * nrows); }
};

namespace coat {

template<>
struct has_custom_base<Table> : std::true_type {};

template<class CC>
struct StructBase<Struct<CC,Table>> {
	Ptr<CC,Value<CC,uint64_t>> operator[](size_t col) const {
		auto &self = static_cast<const Struct<CC,Table>&>(*this);
		//FIXME: loaded every time
		auto vr_nrows = self.template get_value<Table::member_nrows>();
		auto vr_data = self.template get_value<Table::member_data>();
		//coat::Value vr_col(self.cc, col, "col");
		//return vr_data + (vr_nrows *= vr_col);
		return vr_data + (vr_nrows *= col);
	}
};

}

using column_t = std::vector<uint64_t>;


namespace {

static column_t generic(const Table &table, const char *operations){
	const size_t size = table.nrows;
	column_t result(size);
	auto t_start = std::chrono::high_resolution_clock::now();
	for(size_t i=0; i<size; ++i){
		const char *p = operations;
		int col = *p - '0';
		++p;
		uint64_t res = table[col][i];
		while(*p){
			switch(*p){
				case '+':
					col = p[1] - '0';
					res += table[col][i];
					break;
				case '-':
					col = p[1] - '0';
					res -= table[col][i];
					break;
				default:
					printf("unsupported operation: %c\n", *p);
					std::exit(-1);
			}
			p += 2;
		}
		result[i] = res;
	}
	auto t_end = std::chrono::high_resolution_clock::now();
	printf("     generic: %10.2f us\n", std::chrono::duration<double, std::micro>( t_end - t_start).count());
	return result;
}


template<class Fn>
void assemble_jit1(Fn &fn, const char *operations){
	auto args = fn.getArguments("table", "result", "size");
	auto &vr_table = std::get<0>(args);
	auto &vr_result = std::get<1>(args);
	auto &vr_size = std::get<2>(args);

	coat::Value vr_index(fn, 0UL, "index");
	coat::loop_while(fn, vr_index < vr_size, [&]{
		const char *p = operations;
		int col = *p - '0';
		++p;
		auto vr_res = fn.template getValue<uint64_t>("res");
		vr_res = vr_table[col][vr_index];
		while(*p){
			switch(*p){
				case '+':
					col = p[1] - '0';
					vr_res += vr_table[col][vr_index];
					break;
				case '-':
					col = p[1] - '0';
					vr_res -= vr_table[col][vr_index];
					break;
				default:
					printf("unsupported operation: %c\n", *p);
					std::exit(-1);
			}
			p += 2;
		}
		vr_result[vr_index] = vr_res;
		++vr_index;
	});
	coat::ret(fn);
}

template<class Fn>
void assemble_jit2(Fn &fn, const char *operations){
	auto args = fn.getArguments("table", "result", "size");
	auto &vr_table = std::get<0>(args);
	auto &vr_result = std::get<1>(args);
	auto &vr_size = std::get<2>(args);

	auto vr_nrows = vr_table.template get_value<Table::member_nrows>();
	auto vr_data = vr_table.template get_value<Table::member_data>();
	coat::Value vr_index(fn, 0UL, "index");
	coat::loop_while(fn, vr_index < vr_size, [&]{
		const char *p = operations;
		int col = *p - '0';
		++p;
		auto vr_res = fn.template getValue<uint64_t>("res");
		//vr_res = vr_table[col][vr_index];
		auto vr_col = vr_data + (vr_nrows * col);
		vr_res = vr_col[vr_index];
		while(*p){
			switch(*p){
				case '+': {
					col = p[1] - '0';
					//vr_res += vr_table[col][vr_index];
					auto vr_col = vr_data + (vr_nrows * col);
					vr_res += vr_col[vr_index];
					break;
				}
				case '-': {
					col = p[1] - '0';
					//vr_res -= vr_table[col][vr_index];
					auto vr_col = vr_data + (vr_nrows * col);
					vr_res -= vr_col[vr_index];
					break;
				}
				default:
					printf("unsupported operation: %c\n", *p);
					std::exit(-1);
			}
			p += 2;
		}
		vr_result[vr_index] = vr_res;
		++vr_index;
	});
	coat::ret(fn);
}

template<class Fn>
void assemble_jit3(Fn &fn, const char *operations, const Table &table){
	auto args = fn.getArguments("result", "size");
	auto &vr_result = std::get<0>(args);
	auto &vr_size = std::get<1>(args);

	coat::Value vr_index(fn, 0UL, "index");
	coat::loop_while(fn, vr_index < vr_size, [&]{
		const char *p = operations;
		int col = *p - '0';
		++p;
		auto vr_res = fn.template getValue<uint64_t>("res");
		//vr_res = vr_table[col][vr_index];
		const uint64_t *column = table[col];
		auto vr_col = fn.embedValue(column, "col");
		vr_res = vr_col[vr_index];
		while(*p){
			switch(*p){
				case '+': {
					col = p[1] - '0';
					//vr_res += vr_table[col][vr_index];
					const uint64_t *column = table[col];
					auto vr_col = fn.embedValue(column, "col");
					vr_res += vr_col[vr_index];
					break;
				}
				case '-': {
					col = p[1] - '0';
					//vr_res -= vr_table[col][vr_index];
					const uint64_t *column = table[col];
					auto vr_col = fn.embedValue(column, "col");
					vr_res -= vr_col[vr_index];
					break;
				}
				default:
					printf("unsupported operation: %c\n", *p);
					std::exit(-1);
			}
			p += 2;
		}
		vr_result[vr_index] = vr_res;
		++vr_index;
	});
	coat::ret(fn);
}


static column_t jit1_asmjit(const Table &table, const char *operations){
	const size_t size = table.nrows;
	column_t result(size);

	auto t_start = std::chrono::high_resolution_clock::now();

	coat::runtimeasmjit asmrt;
	using func_t = void (*)(const Table *table, uint64_t *result, size_t size);
	coat::Function<coat::runtimeasmjit,func_t> fn(asmrt);
	assemble_jit1(fn, operations);
	// finalize function
	func_t fnptr = fn.finalize();

	auto t_compile = std::chrono::high_resolution_clock::now();
	// execute generated function
	fnptr(&table, result.data(), size);

	asmrt.rt.release(fnptr);

	auto t_end = std::chrono::high_resolution_clock::now();
	printf("jit1  asmjit: %10.2f us (compilation: %10.2f; exec: %10.2f)\n",
		std::chrono::duration<double, std::micro>( t_end - t_start).count(),
		std::chrono::duration<double, std::micro>( t_compile - t_start).count(),
		std::chrono::duration<double, std::micro>( t_end - t_compile).count()
	);

	return result;
}
static column_t jit1_llvmjit(const Table &table, const char *operations){
	const size_t size = table.nrows;
	column_t result(size);

	auto t_start = std::chrono::high_resolution_clock::now();

	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	llvmrt.setOptLevel(2);
	using func_t = void (*)(const Table *table, uint64_t *result, size_t size);
	coat::Function<coat::runtimellvmjit,func_t> fn(llvmrt);
	assemble_jit1(fn, operations);

	llvmrt.print("jit1.ll");
	if(!llvmrt.verifyFunctions()){
		puts("verification failed. aborting.");
		exit(EXIT_FAILURE); //FIXME: better error handling
	}
	if(llvmrt.getOptLevel() > 0){
		llvmrt.optimize();
		llvmrt.print("jit1_opt.ll");
		if(!llvmrt.verifyFunctions()){
			puts("verification after optimization failed. aborting.");
			exit(EXIT_FAILURE); //FIXME: better error handling
		}
	}
	// finalize function
	func_t fnptr = fn.finalize();

	auto t_compile = std::chrono::high_resolution_clock::now();
	// execute generated function
	fnptr(&table, result.data(), size);

	auto t_end = std::chrono::high_resolution_clock::now();
	printf("jit1 llvmjit: %10.2f us (compilation: %10.2f; exec: %10.2f)\n",
		std::chrono::duration<double, std::micro>( t_end - t_start).count(),
		std::chrono::duration<double, std::micro>( t_compile - t_start).count(),
		std::chrono::duration<double, std::micro>( t_end - t_compile).count()
	);

	return result;
}

static column_t jit2_asmjit(const Table &table, const char *operations){
	const size_t size = table.nrows;
	column_t result(size);

	auto t_start = std::chrono::high_resolution_clock::now();

	coat::runtimeasmjit asmrt;
	using func_t = void (*)(const Table *table, uint64_t *result, size_t size);
	coat::Function<coat::runtimeasmjit,func_t> fn(asmrt);
	assemble_jit2(fn, operations);
	// finalize function
	func_t fnptr = fn.finalize();

	auto t_compile = std::chrono::high_resolution_clock::now();
	// execute generated function
	fnptr(&table, result.data(), size);

	asmrt.rt.release(fnptr);

	auto t_end = std::chrono::high_resolution_clock::now();
	printf("jit2  asmjit: %10.2f us (compilation: %10.2f; exec: %10.2f)\n",
		std::chrono::duration<double, std::micro>( t_end - t_start).count(),
		std::chrono::duration<double, std::micro>( t_compile - t_start).count(),
		std::chrono::duration<double, std::micro>( t_end - t_compile).count()
	);

	return result;
}
static column_t jit2_llvmjit(const Table &table, const char *operations){
	const size_t size = table.nrows;
	column_t result(size);

	auto t_start = std::chrono::high_resolution_clock::now();

	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	llvmrt.setOptLevel(2);
	using func_t = void (*)(const Table *table, uint64_t *result, size_t size);
	coat::Function<coat::runtimellvmjit,func_t> fn(llvmrt);
	assemble_jit2(fn, operations);

	llvmrt.print("jit2.ll");
	if(!llvmrt.verifyFunctions()){
		puts("verification failed. aborting.");
		exit(EXIT_FAILURE); //FIXME: better error handling
	}
	if(llvmrt.getOptLevel() > 0){
		llvmrt.optimize();
		llvmrt.print("jit2_opt.ll");
		if(!llvmrt.verifyFunctions()){
			puts("verification after optimization failed. aborting.");
			exit(EXIT_FAILURE); //FIXME: better error handling
		}
	}
	// finalize function
	func_t fnptr = fn.finalize();

	auto t_compile = std::chrono::high_resolution_clock::now();
	// execute generated function
	fnptr(&table, result.data(), size);

	auto t_end = std::chrono::high_resolution_clock::now();
	printf("jit2 llvmjit: %10.2f us (compilation: %10.2f; exec: %10.2f)\n",
		std::chrono::duration<double, std::micro>( t_end - t_start).count(),
		std::chrono::duration<double, std::micro>( t_compile - t_start).count(),
		std::chrono::duration<double, std::micro>( t_end - t_compile).count()
	);

	return result;
}

static column_t jit3_asmjit(const Table &table, const char *operations){
	const size_t size = table.nrows;
	column_t result(size);

	auto t_start = std::chrono::high_resolution_clock::now();

	coat::runtimeasmjit asmrt;
	using func_t = void (*)(uint64_t *result, size_t size);
	coat::Function<coat::runtimeasmjit,func_t> fn(asmrt);
	assemble_jit3(fn, operations, table);
	// finalize function
	func_t fnptr = fn.finalize();

	auto t_compile = std::chrono::high_resolution_clock::now();
	// execute generated function
	fnptr(result.data(), size);

	asmrt.rt.release(fnptr);

	auto t_end = std::chrono::high_resolution_clock::now();
	printf("jit3  asmjit: %10.2f us (compilation: %10.2f; exec: %10.2f)\n",
		std::chrono::duration<double, std::micro>( t_end - t_start).count(),
		std::chrono::duration<double, std::micro>( t_compile - t_start).count(),
		std::chrono::duration<double, std::micro>( t_end - t_compile).count()
	);

	return result;
}
static column_t jit3_llvmjit(const Table &table, const char *operations){
	const size_t size = table.nrows;
	column_t result(size);

	auto t_start = std::chrono::high_resolution_clock::now();

	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	llvmrt.setOptLevel(2);
	using func_t = void (*)(uint64_t *result, size_t size);
	coat::Function<coat::runtimellvmjit,func_t> fn(llvmrt);
	assemble_jit3(fn, operations, table);

	llvmrt.print("jit3.ll");
	if(!llvmrt.verifyFunctions()){
		puts("verification failed. aborting.");
		exit(EXIT_FAILURE); //FIXME: better error handling
	}
	if(llvmrt.getOptLevel() > 0){
		llvmrt.optimize();
		llvmrt.print("jit3_opt.ll");
		if(!llvmrt.verifyFunctions()){
			puts("verification after optimization failed. aborting.");
			exit(EXIT_FAILURE); //FIXME: better error handling
		}
	}
	// finalize function
	func_t fnptr = fn.finalize();

	auto t_compile = std::chrono::high_resolution_clock::now();
	// execute generated function
	fnptr(result.data(), size);

	auto t_end = std::chrono::high_resolution_clock::now();
	printf("jit3 llvmjit: %10.2f us (compilation: %10.2f; exec: %10.2f)\n",
		std::chrono::duration<double, std::micro>( t_end - t_start).count(),
		std::chrono::duration<double, std::micro>( t_compile - t_start).count(),
		std::chrono::duration<double, std::micro>( t_end - t_compile).count()
	);

	return result;
}


static void write(const column_t &result, const char *name){
	FILE *fd = fopen(name, "w");
	for(uint64_t r : result){
		fprintf(fd, "%lu\n", r);
	}
	fclose(fd);
}

} // anonymous namespace


int main(int argc, char **argv){
	// defaults
	size_t cols = 5;
	size_t rows = 8*1024*1024;
	size_t iterations = 1;
#define REPEAT for(size_t i=0; i<iterations; ++i)
	const char *operations = "2+1-0+4-3";
	bool dump=false;

	int opt;
	while((opt=getopt(argc, argv, "hdc:r:i:o:")) != -1){
		switch(opt){
			case 'h':
				printf("\
%s [options]\n\
	-c number   number of columns\n\
	-r number   number of rows\n\
	-i number   number of iterations of each measurement\n\
	-o string   sequence of operations on the columns for each row, e.g., '2+1-0+4-3'\n\
	-d          enable dumping of results to *.dump files\n", argv[0]);
				return 0;
			case 'd': dump = true; break;
			case 'c': cols = atoi(optarg); break;
			case 'r': rows = atoi(optarg); break;
			case 'i': iterations = atoi(optarg); break;
			case 'o': operations = optarg; break;
		}
	}
	printf("cols: %lu\nrows: %lu\niterations: %lu\noperations: %s\n\n", cols, rows, iterations, operations);

	std::mt19937 gen(42);
	Table table(cols, rows);
	for(size_t i=0;i<cols; ++i){
		std::iota(table[i], table[i] + table.nrows, 0);
		std::shuffle(table[i], table[i] + table.nrows, gen);
	}

	REPEAT{
		column_t result = generic(table, operations);
		if(dump) write(result, "calc_generic.dump");
	}

	REPEAT{
		column_t result = jit1_asmjit(table, operations);
		if(dump) write(result, "calc_jit1_asmjit.dump");
	}
	REPEAT{
		column_t result = jit1_llvmjit(table, operations);
		if(dump) write(result, "calc_jit1_llvmjit.dump");
	}

	REPEAT{
		column_t result = jit2_asmjit(table, operations);
		if(dump) write(result, "calc_jit2_asmjit.dump");
	}
	REPEAT{
		column_t result = jit2_llvmjit(table, operations);
		if(dump) write(result, "calc_jit2_llvmjit.dump");
	}

	REPEAT{
		column_t result = jit3_asmjit(table, operations);
		if(dump) write(result, "calc_jit3_asmjit.dump");
	}
	REPEAT{
		column_t result = jit3_llvmjit(table, operations);
		if(dump) write(result, "calc_jit3_llvmjit.dump");
	}

	return 0;
}
