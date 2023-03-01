#ifndef COAT_LLVMJIT_FUNCTION_H_
#define COAT_LLVMJIT_FUNCTION_H_

#include "../runtimellvmjit.h"

#include <tuple>   // apply
#include <vector>
#include <fstream> // ofstream
#include <cstdio>  // fprintf

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LegacyPassManager.h> // for optimizations
#include <llvm/IR/Verifier.h>

#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include <llvm/Support/raw_ostream.h>

#include "DebugOperand.h"
#include <llvm/IR/DIBuilder.h>


namespace coat {

//HACK: would love to not need this stuff for addFunction
template<typename T>
struct getFunctionType;

template<typename R, typename ...Args>
struct getFunctionType<R(*)(Args...)>{
	llvm::FunctionType *operator()(llvm::LLVMContext &context){
		return llvm::FunctionType::get(
			getLLVMType<std::remove_cv_t<R>>(context),
			{(getLLVMType<std::remove_cv_t<Args>>(context))...},
			false
		);
	}
};


struct LLVMBuilders {
	llvm::IRBuilder<> ir;
#ifdef LLVMJIT_DEBUG
	llvm::DIBuilder dbg;
	llvm::DIScope *debugScope;
	bool isDebugFinalized = false;

	LLVMBuilders(llvm::LLVMContext &ctx, llvm::Module &M) : ir(ctx), dbg(M) {}
#else
	LLVMBuilders(llvm::LLVMContext &ctx, llvm::Module &) : ir(ctx) {}
#endif

	void debugFinalize(){
#ifdef LLVMJIT_DEBUG
		if(!isDebugFinalized){
			dbg.finalize();
			isDebugFinalized = true;
		}
#endif
	}
};

template<typename R, typename ...Args>
struct Function<runtimellvmjit,R(*)(Args...)>{
	using CC = runtimellvmjit;
	using F = LLVMBuilders;
	using func_type = R (*)(Args...);
	using return_type = R;

	runtimellvmjit &jit;
	std::unique_ptr<llvm::LLVMContext> context;
	std::unique_ptr<llvm::Module> M;
	LLVMBuilders cc;

	const char *name;
	llvm::Function *func; //TODO: remove, first in functions
	// all functions in module, TODO: what about this?
	std::vector<llvm::Function*> functions;

	Function(
		runtimellvmjit &jit, const char *name="func"
#ifdef LLVMJIT_DEBUG
		,const char *srcfile=__builtin_FILE(), int srcline=__builtin_LINE()
#endif
	)
		: jit(jit)
		, context(std::make_unique<llvm::LLVMContext>())
		, M(std::make_unique<llvm::Module>("test", *context))
		, cc(*context, *M)
		, name(name)
	{
#ifdef LLVMJIT_DEBUG
		// source file where function was created
		// a bit pointless as the code generating a function may be spread across multiple source files
		llvm::DIFile *difile = cc.dbg.createFile(
			srcfile, "." //TODO: how to get directory?
		);
		//TODO: change bool when optimization is enabled
		/*llvm::DICompileUnit *dicu = */cc.dbg.createCompileUnit(llvm::dwarf::DW_LANG_C, difile, "COAT", false, "", 0);
#endif

		// LLVM IR function type
		llvm::FunctionType *jit_func_type = llvm::FunctionType::get(
			getLLVMType<std::remove_cv_t<R>>(*context),
			{(getLLVMType<std::remove_cv_t<Args>>(*context))...},
			false
		);
		// LLVM IR function definition
		func = createFunction(jit_func_type, name); // function name

#ifdef LLVMJIT_DEBUG
		// function type in debug info
		llvm::DISubroutineType *dbg_func_type = cc.dbg.createSubroutineType(
			cc.dbg.getOrCreateTypeArray({
				getDebugType<std::remove_cv_t<R>>(cc.dbg, difile),
				getDebugType<std::remove_cv_t<Args>>(cc.dbg, difile)...
			})
		);
		// debug definition of function
		llvm::DISubprogram *disubprogram = cc.dbg.createFunction(
			difile /*scope*/,
			name, name /*linker name, mangled?*/,
			difile, srcline,
			dbg_func_type,
			srcline /*scope line ???*/,
			llvm::DINode::DIFlags::FlagPrototyped,
			llvm::DISubprogram::SPFlagDefinition
		);
		func->setSubprogram(disubprogram);
		cc.debugScope = disubprogram;
#endif

		llvm::BasicBlock *bb_prolog = llvm::BasicBlock::Create(*context, "prolog", func);
		llvm::BasicBlock *bb_start = llvm::BasicBlock::Create(*context, "start", func);
		cc.ir.SetInsertPoint(bb_prolog);
		cc.ir.CreateBr(bb_start);

		cc.ir.SetInsertPoint(bb_start);

#ifdef LLVMJIT_DEBUG
		// unset location for prologue
		cc.ir.SetCurrentDebugLocation(llvm::DebugLoc());
#endif
	}
	Function(const Function &) = delete;


	template<typename FuncSig>
	InternalFunction<runtimellvmjit,FuncSig> addFunction(const char *name){
		llvm::FunctionType *func_type = getFunctionType<FuncSig>{}(*context);
		llvm::Function *foo = createFunction(func_type, name, llvm::Function::InternalLinkage);
		return InternalFunction<runtimellvmjit,FuncSig>(jit, cc, name, foo);
	}

	template<class IFunc>
	void startNextFunction(const IFunc &internalCall){
		// start new basic block in internal function
		llvm::BasicBlock *bb_prolog = llvm::BasicBlock::Create(*context, "prolog", internalCall.func);
		llvm::BasicBlock *bb_start = llvm::BasicBlock::Create(*context, "start", internalCall.func);
		// start inserting here
		cc.ir.SetInsertPoint(bb_prolog);
		cc.ir.CreateBr(bb_start);
		cc.ir.SetInsertPoint(bb_start);
	}


//FIXME: too much duplicated code
#ifdef LLVMJIT_DEBUG
	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments_impl(Names... names, const char *file, int line) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough or too many names specified");
		// create all parameter wrapper objects in a tuple
		std::tuple<wrapper_type<F,Args>...> ret { wrapper_type<F,Args>(cc, names, true, file, line)... };

		// get argument value and put it in wrapper object
		std::apply(
			[&](auto &&...args){
				llvm::Function *fn = cc.ir.GetInsertBlock()->getParent();
				llvm::Function::arg_iterator arguments = fn->arg_begin();
				// set debug location for all assignments
				cc.ir.SetCurrentDebugLocation(llvm::DebugLoc::get(line, 0, cc.debugScope));
				// (tuple_at_0 = (llvm::Value*)args++), (tuple_at_1 = (llvm::Value*)args++), ... ;
				((args = (llvm::Value*)arguments++), ...);
			},
			ret
		);
		return ret;
	}
	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(D2<const char*> first, Names... rest) {
		return getArguments_impl<const char*, Names...>(first.operand, rest..., first.file, first.line);
	}
#else
	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(Names... names) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough or too many names specified");
		// create all parameter wrapper objects in a tuple
		std::tuple<wrapper_type<F,Args>...> ret { wrapper_type<F,Args>(cc, names)... };

		// get argument value and put it in wrapper object
		std::apply(
			[&](auto &&...args){
				llvm::Function *fn = cc.ir.GetInsertBlock()->getParent();
				llvm::Function::arg_iterator arguments = fn->arg_begin();
				// (tuple_at_0 = (llvm::Value*)args++), (tuple_at_1 = (llvm::Value*)args++), ... ;
				((args = (llvm::Value*)arguments++), ...);
			},
			ret
		);
		return ret;
	}
#endif

	//HACK: trying factory
	template<typename T>
	Value<F,T> getValue(const char *name="") {
		return Value<F,T>(cc, name);
	}
	// embed value in the generated code, returns wrapper initialized to this value
	template<typename T>
	wrapper_type<F,T> embedValue(T value, const char *name=""){
		return wrapper_type<F,T>(cc, value, name);
	}

	func_type finalize(){
		// finalize debug information
		cc.debugFinalize();

		// iterate over all functions in the module
		for(auto &f : M->functions()){
			// check if it has a function pointer attached as metadata
			if(llvm::MDNode *md = f.getMetadata("coat.fnptr")){
				// avoid defining a symbol twice by looking first
				bool found = jit.J->lookup(f.getName());
				if(!found){
					//FIXME: get rid of llvm::cast
					llvm::Constant *c = llvm::cast<llvm::ConstantAsMetadata>(md->getOperand(0))->getValue();
					uint64_t fnptr = llvm::cast<llvm::ConstantInt>(c)->getZExtValue();
					// add function address as absolute symbol
					llvm::cantFail(jit.J->getMainJITDylib().define(
						llvm::orc::absoluteSymbols({{
							jit.J->mangleAndIntern(f.getName()),
							llvm::JITEvaluatedSymbol::fromPointer((void*)fnptr)
						}})
					));
				}
			}
		}

		llvm::orc::ThreadSafeModule tsm(std::move(M), std::move(context));
        llvm::cantFail(jit.J->addIRModule(std::move(tsm)));
		// Look up the JIT'd function
		llvm::JITEvaluatedSymbol SumSym = llvm::cantFail(jit.J->lookup(name));
		// get function ptr
		return (func_type)SumSym.getAddress();
	}

	operator const llvm::IRBuilder<>&() const { return cc.ir; }
	operator       llvm::IRBuilder<>&()       { return cc.ir; }
	operator const LLVMBuilders&() const { return cc; }
	operator       LLVMBuilders&()       { return cc; }

	void optimize(int optLevel){
		// finalize debug information
		cc.debugFinalize();

		//TODO: use TransformLayer instead
		llvm::PassManagerBuilder pm_builder;
		pm_builder.OptLevel = optLevel;
		pm_builder.SizeLevel = 0;
		//pm_builder.Inliner = llvm::createAlwaysInlinerLegacyPass();
		pm_builder.Inliner = llvm::createFunctionInliningPass(optLevel, 0, false);
		pm_builder.LoopVectorize = true;
		pm_builder.SLPVectorize = true;

		//pm_builder.VerifyInput = true;
		pm_builder.VerifyOutput = true;

		llvm::legacy::FunctionPassManager function_pm(M.get());
		llvm::legacy::PassManager module_pm;
		pm_builder.populateFunctionPassManager(function_pm);
		pm_builder.populateModulePassManager(module_pm);

		function_pm.doInitialization();
		//TODO: multiple functions
		//for(llvm::Function *f : functions){
		//	function_pm.run(*f);
		//}
		function_pm.run(*func);

		module_pm.run(*M);
	}

	// print IR to file
	void printIR(const char *fname){
		// finalize debug information
		cc.debugFinalize();

		std::string str;
		llvm::raw_string_ostream os(str);
		M->print(os, nullptr);
		std::ofstream of(fname);
		of << os.str();
	}

	//TODO: port to LLVM 11
#if 0
	void dumpAssembly(const char *filename){
		std::string str;
		{
			llvm::legacy::PassManager pm;
			llvm::raw_string_ostream os(str);
			llvm::buffer_ostream pstream(os);
			tm->Options.MCOptions.AsmVerbose = true; // get some comments linking it to LLVM IR
			tm->addPassesToEmitFile(pm, pstream, nullptr, llvm::TargetMachine::CGFT_AssemblyFile);
			pm.run(*module);
		}
		std::ofstream of(filename);
		of << str;
	}
#endif

	bool verify(){
		// finalize debug information
		cc.debugFinalize();

		std::string str;
		llvm::raw_string_ostream os(str);
		bool failed = llvm::verifyFunction(*func, &os);
		if(failed){
			fprintf(stderr, "\nfunction verifier:\n%s\n", os.str().c_str());
		}
		return !failed;
	}

private:
	// create function in LLVM and add to functions vector
	llvm::Function *createFunction(llvm::FunctionType *jit_func_type, const char *name, llvm::GlobalValue::LinkageTypes linkage=llvm::Function::ExternalLinkage){
		llvm::Function *foo = llvm::Function::Create(jit_func_type, linkage, name, M.get());
		functions.push_back(foo);
		return foo;
	}
};


template<typename R, typename ...Args>
struct InternalFunction<runtimellvmjit,R(*)(Args...)>{
	using F = LLVMBuilders;
	using func_type = R (*)(Args...);
	using return_type = R;

	LLVMBuilders &cc;
	const char *name;
	llvm::Function *func;

	//TODO: make private, Function::addFunction should be used
	InternalFunction(runtimellvmjit & /*jit*/, LLVMBuilders &cc, const char *name, llvm::Function *func)
		: cc(cc), name(name), func(func) {}
	InternalFunction(const InternalFunction &other)
		: cc(other.cc), name(other.name), func(other.func) {}


	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(Names... names) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough or too many names specified");
		// create all parameter wrapper objects in a tuple
		std::tuple<wrapper_type<F,Args>...> ret { wrapper_type<F,Args>(cc, names)... };
		// get argument value and put it in wrapper object
		std::apply(
			[&](auto &&...args){
				llvm::Function *fn = cc.ir.GetInsertBlock()->getParent();
				llvm::Function::arg_iterator arguments = fn->arg_begin();
				// (tuple_at_0 = (llvm::Value*)args++), (tuple_at_1 = (llvm::Value*)args++), ... ;
				((args = (llvm::Value*)arguments++), ...);
			},
			ret
		);
		return ret;
	}

	operator const llvm::IRBuilder<>&() const { return cc.ir; }
	operator       llvm::IRBuilder<>&()       { return cc.ir; }
	operator const LLVMBuilders&() const { return cc; }
	operator       LLVMBuilders&()       { return cc; }
};


// helper function
static inline llvm::Value *allocateStackVariable(llvm::IRBuilder<> &cc, llvm::Type *ty, const char *name){
	llvm::BasicBlock *bb_current = cc.GetInsertBlock();
	llvm::BasicBlock &bb_prolog = bb_current->getParent()->getEntryBlock();
	llvm::BasicBlock::iterator it = bb_prolog.end();
	cc.SetInsertPoint(&bb_prolog, --it);
	llvm::Value *ret = cc.CreateAlloca(ty, nullptr, name);
	cc.SetInsertPoint(bb_current);
	return ret;
}

} // namespace

#endif
