#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <chrono>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>


#include "coat/Function.h"
#include "coat/ControlFlow.h"

#include "coat/datastructs/pod_vector.h"


using column_t = uint64_t*;

class Relation {
private:
	char *mapped_addr; // address returned by mmap()
	uint64_t fsize;
	uint64_t size; // number of tuples
	std::vector<column_t> columns;

public:
	Relation(const char *fname){
		int fd = open(fname, O_RDONLY);
		if(fd == -1){
			perror("failed to open relation");
			exit(EXIT_FAILURE);
		}
		// get file size
		struct stat s;
		if(fstat(fd, &s) == -1){
			perror("failed to fstat file");
			exit(EXIT_FAILURE);
		}
		fsize = s.st_size;
		// no magic number to indicate file type, needs at least header size
		if(fsize < 16u){
			fprintf(stderr, "relation file %s does not contain a valid header\n", fname);
			exit(EXIT_FAILURE);
		}
		// map file into memory
		mapped_addr = reinterpret_cast<char*>(mmap(nullptr, fsize, PROT_READ, MAP_PRIVATE, fd, 0));
		if(mapped_addr == MAP_FAILED){
			perror("failed to fstat file");
			exit(EXIT_FAILURE);
		}
		// read header
		char *addr = mapped_addr;
		size = *reinterpret_cast<uint64_t*>(addr);
		addr += sizeof(uint64_t);
		size_t num_columns = *reinterpret_cast<size_t*>(addr);
		addr += sizeof(size_t);
		columns.reserve(num_columns);
		for(size_t i=0; i<num_columns; ++i){
			// initially 64-bit values
			columns.emplace_back(reinterpret_cast<uint64_t*>(addr));
			addr += size*sizeof(uint64_t);
		}
		// close file, undo open(), file still open because of mmap()
		close(fd);
	}
	~Relation(){
		munmap(mapped_addr, fsize);
	}
	Relation(const Relation&)=delete;
	Relation(Relation &&o){
		mapped_addr = o.mapped_addr;
		fsize = o.fsize;
		size = o.size;
		columns = std::move(o.columns);
		o.mapped_addr = nullptr;
		o.fsize = 0;
		o.size = 0;
	}

	uint64_t getNumberOfTuples() const{
		return size;
	}
	const column_t &getColumn(int col) const{
		return columns[col];
	}
	size_t getNumberOfColumns() const{
		return columns.size();
	}
};


#if 0
template<class CC>
struct CodgenContext {
	std::vector<Value<CC,uint64_t>> rowids;
	coat::Value<CC,uint64_t> upper;
	//coat::Ptr<CC,coat::Value<CC,uint64_t>> projection_addr;
	Value<CC,uint64_t> amount;
};
using Ctx_asmjit = CodegenContext<asmjit::x86::Compiler>;
using Ctx_llvmjit = CodegenContext<llvm::IRBuilder<>>;
#endif

// function signature of generated function
// parameters: lower, upper (morsel), ptr to buffer of projection entries
// returns number of results
//using codegen_func_type = uint64_t (*)(uint64_t,uint64_t,uint64_t*);
using codegen_func_type = void (*)();
#ifdef ENABLE_ASMJIT
using Fn_asmjit = coat::Function<coat::runtimeasmjit,codegen_func_type>;
#endif
#ifdef ENABLE_LLVMJIT
using Fn_llvmjit = coat::Function<coat::runtimellvmjit,codegen_func_type>;
#endif


class Operator{
protected:
	Operator *next=nullptr;

public:
	Operator(){}
	virtual ~Operator(){
		delete next;
	}

	void setNext(Operator *next){
		this->next = next;
	}

	// tuple-by-tuple execution
	virtual void execute(uint64_t rowid)=0;

	// code generation with coat, for each backend, chosen at runtime
#ifdef ENABLE_ASMJIT
	virtual void codegen(Fn_asmjit &fn, coat::Value<asmjit::x86::Compiler,uint64_t> &rowid)=0;
#endif
#ifdef ENABLE_LLVMJIT
	virtual void codegen(Fn_llvmjit &fn, coat::Value<llvm::IRBuilder<>,uint64_t> &rowid)=0;
#endif
};


class ScanOperator final : public Operator{
private:
	size_t tuples;
	template<class Fn, class CC>
	void codegen_impl(Fn &fn, coat::Value<CC,uint64_t> &rowid){
		coat::Value vr_tuples(fn, tuples, "tuples");
		coat::do_while(fn, [&]{
			next->codegen(fn, rowid);
			++rowid;
		}, rowid < vr_tuples);
	}

public:
	ScanOperator(const Relation &relation) : tuples(relation.getNumberOfTuples()) {}

	void execute(uint64_t) override {
		for(uint64_t idx=0; idx<tuples; ++idx){
			// pass tuple by tuple (very bad performance without codegen)
			next->execute(idx);
		}
	}

#ifdef ENABLE_ASMJIT
	void codegen(Fn_asmjit &fn, coat::Value<asmjit::x86::Compiler,uint64_t> &rowid) override { codegen_impl(fn, rowid); }
#endif
#ifdef ENABLE_LLVMJIT
	void codegen(Fn_llvmjit &fn, coat::Value<llvm::IRBuilder<>,uint64_t> &rowid) override { codegen_impl(fn, rowid); }
#endif
};



class FilterOperator final : public Operator{
private:
	const column_t &column;
	uint64_t constant;
	enum Comparison {
		Less, Greater, Equal
	} comparison;

	template<class Fn, class CC>
	void codegen_impl(Fn &fn, coat::Value<CC,uint64_t> &rowid){
		coat::Ptr<CC, coat::Value<CC,uint64_t>> col(fn, "col");
		col = column;
		auto val = col[rowid];
		switch(comparison){
			case Comparison::Less: {
				coat::if_then(fn, val < constant, [&]{
					next->codegen(fn, rowid);
				});
				break;
			}
			case Comparison::Greater: {
				coat::if_then(fn, val > constant, [&]{
					next->codegen(fn, rowid);
				});
				break;
			}
			case Comparison::Equal: {
				coat::if_then(fn, val == constant, [&]{
					next->codegen(fn, rowid);
				});
				break;
			}
		}
	}

public:
	FilterOperator(const Relation &rel, const char *filter_string)
		: column(rel.getColumn(filter_string[0] - '0'))
	{
		//int colid = '0' - filter_string[0]; //HACK
		//column = rel.getColumn(colid);
		constant = strtoul(&filter_string[2], nullptr, 10);
		switch(filter_string[1]){
			case '<':
			case 'l':
				comparison = Comparison::Less;
				break;

			case '>':
			case 'g':
				comparison = Comparison::Greater;
				break;

			case '=':
			case 'e':
				comparison = Comparison::Equal;
				break;
		}
	}

	void execute(uint64_t rowid) override {
		uint64_t val = column[rowid];
		switch(comparison){
			case Comparison::Less: {
				if(val < constant){
					next->execute(rowid);
				}
				break;
			}
			case Comparison::Greater: {
				if(val > constant){
					next->execute(rowid);
				}
				break;
			}
			case Comparison::Equal: {
				if(val == constant){
					next->execute(rowid);
				}
				break;
			}
		}
	}

#ifdef ENABLE_ASMJIT
	void codegen(Fn_asmjit &fn, coat::Value<asmjit::x86::Compiler,uint64_t> &rowid) override { codegen_impl(fn, rowid); }
#endif
#ifdef ENABLE_LLVMJIT
	void codegen(Fn_llvmjit &fn, coat::Value<llvm::IRBuilder<>,uint64_t> &rowid) override { codegen_impl(fn, rowid); }
#endif
};


class ProjectionOperator final : public Operator {
private:
	pod_vector<uint64_t> results;

	template<class Fn, class CC>
	void codegen_impl(Fn &fn, coat::Value<CC,uint64_t> &rowid){
		coat::Struct<CC,decltype(results)> vr_results(fn, "results");
		vr_results = &results;
		vr_results.push_back(rowid);
	}

public:
	void execute(uint64_t rowid) override {
		results.emplace_back(rowid);
	}

#ifdef ENABLE_ASMJIT
	void codegen(Fn_asmjit &fn, coat::Value<asmjit::x86::Compiler,uint64_t> &rowid) override { codegen_impl(fn, rowid); }
#endif
#ifdef ENABLE_LLVMJIT
	void codegen(Fn_llvmjit &fn, coat::Value<llvm::IRBuilder<>,uint64_t> &rowid) override { codegen_impl(fn, rowid); }
#endif

	size_t getResultSize() const {
		return results.size();
	}
};


// table in columnar storage, imported from CSV, all columns integers
// filter predicates for various columns "colid comparison literal", example: 1<42
// all predicates have to hold
// return list of rowids which satisfy predicates
//
// if( pred1 && pred2 && ... )
//
// if( pred1 )
//   if( pred2 )
//     if( ... )
//       ...
//
// for( auto &pred : predicates)
//   

int main(int argc, char **argv){
	if(argc == 1){
		printf("usage: %s [ -t | -a | -l[0-3] ] relation_file filer...\n", argv[0]);
		return 0;
	}

	printf("loading %s...\n", argv[2]);
	Relation table(argv[2]);

	printf("columns: %lu; tuples: %lu\n", table.getNumberOfColumns(), table.getNumberOfTuples());

	ScanOperator *scan = new ScanOperator(table);
	Operator *op = scan;
	for(int i=3; i<argc; ++i){
		printf("%s\n", argv[i]);
		FilterOperator *filter = new FilterOperator(table, argv[i]);
		op->setNext(filter);
		op = filter;
	}
	ProjectionOperator *proj = new ProjectionOperator();
	op->setNext(proj);

	if(argv[1][1] == 't'){
		puts("tuple-at-a-time");
		auto t_start = std::chrono::high_resolution_clock::now();

		// run
		scan->execute(0);

		auto t_end = std::chrono::high_resolution_clock::now();
		printf("time: %12.2f us\n", std::chrono::duration<double, std::micro>( t_end - t_start).count());
		printf("results size: %lu\n", proj->getResultSize());
#ifdef ENABLE_ASMJIT
	}else if(argv[1][1] == 'a'){
		puts("asmjit backend");
		// init backend
		coat::runtimeasmjit asmrt;

		auto t_start = std::chrono::high_resolution_clock::now();
		coat::Function<coat::runtimeasmjit,codegen_func_type> fn(&asmrt);
		{
			coat::Value rowid(fn, 0UL, "rowid");
			scan->codegen(fn, rowid);
			coat::ret(fn);
		}
		// finalize function
		codegen_func_type fnptr = fn.finalize(&asmrt);

		auto t_codegen_done = std::chrono::high_resolution_clock::now();

		// execute generated function
		fnptr();

		auto t_end = std::chrono::high_resolution_clock::now();
		printf("codegen: %12.f us\nexecute: %12.f us\n  total: %12.2f us\n",
			std::chrono::duration<double, std::micro>( t_codegen_done - t_start).count(),
			std::chrono::duration<double, std::micro>( t_end - t_codegen_done).count(),
			std::chrono::duration<double, std::micro>( t_end - t_start).count()
		);
		printf("results size: %lu\n", proj->getResultSize());

		asmrt.rt.release(fnptr);
#endif
#ifdef ENABLE_LLVMJIT
	}else if(argv[1][1] == 'l'){
		puts("llvm backend");
		// init backend
		coat::runtimellvmjit::initTarget();
		coat::runtimellvmjit llvmrt;
		bool optimize=false;
		if(argv[1][2]){
			switch(argv[1][2]){
				case '0': llvmrt.setOptLevel(0); break;
				case '1': llvmrt.setOptLevel(1); optimize = true; break;
				case '2': llvmrt.setOptLevel(2); optimize = true; break;
				case '3': llvmrt.setOptLevel(3); optimize = true; break;
			}
		}

		auto t_start = std::chrono::high_resolution_clock::now();
		coat::Function<coat::runtimellvmjit,codegen_func_type> fn(llvmrt);
		{
			coat::Value rowid(fn, 0UL, "rowid");
			scan->codegen(fn, rowid);
			coat::ret(fn);
		}
		auto t_codegen = std::chrono::high_resolution_clock::now();
		llvmrt.print("filter.ll");
		llvmrt.verifyFunctions();
		if(optimize){
			llvmrt.optimize();
			llvmrt.print("filter_opt.ll");
			llvmrt.verifyFunctions();
		}
		// finalize function
		codegen_func_type fnptr = fn.finalize(llvmrt);

		auto t_codegen_done = std::chrono::high_resolution_clock::now();

		// execute generated function
		fnptr();

		auto t_end = std::chrono::high_resolution_clock::now();
		printf(" codegen: %12.f us\noptimize: %12.f us\n execute: %12.f us\n   total: %12.2f us\n",
			std::chrono::duration<double, std::micro>( t_codegen - t_start).count(),
			std::chrono::duration<double, std::micro>( t_codegen_done - t_codegen).count(),
			std::chrono::duration<double, std::micro>( t_end - t_codegen_done).count(),
			std::chrono::duration<double, std::micro>( t_end - t_start).count()
		);
		printf("results size: %lu\n", proj->getResultSize());
		//FIXME: free function
#endif
	}

	delete scan;


	return 0;
}
