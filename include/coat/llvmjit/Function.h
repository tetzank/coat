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


template<typename R, typename ...Args>
struct Function<runtimellvmjit,R(*)(Args...)>{
	using CC = runtimellvmjit;
	using F = ::llvm::IRBuilder<>;
	using func_type = R (*)(Args...);
	using return_type = R;

	runtimellvmjit &jit;
	std::unique_ptr<llvm::LLVMContext> context;
	std::unique_ptr<llvm::Module> M;
	llvm::IRBuilder<> cc;

	const char *name;
	llvm::Function *func; //TODO: remove, first in functions
	// all functions in module, TODO: what about this?
	std::vector<llvm::Function*> functions;

	Function(runtimellvmjit &jit, const char *name="func")
		: jit(jit)
		, context(std::make_unique<llvm::LLVMContext>())
		, M(std::make_unique<llvm::Module>("test", *context))
		, cc(*context)
		, name(name)
	{
		llvm::FunctionType *jit_func_type = llvm::FunctionType::get(
			getLLVMType<std::remove_cv_t<R>>(*context),
			{(getLLVMType<std::remove_cv_t<Args>>(*context))...},
			false
		);
		func = createFunction(jit_func_type, name); // function name

		llvm::BasicBlock *bb_prolog = llvm::BasicBlock::Create(*context, "prolog", func);
		llvm::BasicBlock *bb_start = llvm::BasicBlock::Create(*context, "start", func);
		cc.SetInsertPoint(bb_prolog);
		cc.CreateBr(bb_start);

		cc.SetInsertPoint(bb_start);
	}
	Function(const Function &) = delete;


	//TODO: make private
	llvm::Function *createFunction(llvm::FunctionType *jit_func_type, const char *name, llvm::GlobalValue::LinkageTypes linkage=llvm::Function::ExternalLinkage){
		llvm::Function *foo = llvm::Function::Create(jit_func_type, linkage, name, M.get());
		functions.push_back(foo);
		return foo;
	}


	template<typename FuncSig>
	InternalFunction<runtimellvmjit,FuncSig> addFunction(const char *name){
		llvm::FunctionType *func_type = getFunctionType<FuncSig>{}(*context);
		llvm::Function *foo = createFunction(func_type, name, llvm::Function::InternalLinkage);
		return InternalFunction<runtimellvmjit,FuncSig>(jit, cc, name, foo);
	}

	template<class IFunc>
	void startNextFunction(const IFunc &internalCall){
		// start new basic block in internal function
		llvm::BasicBlock *bb_prolog = llvm::BasicBlock::Create(cc.getContext(), "prolog", internalCall.func);
		llvm::BasicBlock *bb_start = llvm::BasicBlock::Create(cc.getContext(), "start", internalCall.func);
		// start inserting here
		cc.SetInsertPoint(bb_prolog);
		cc.CreateBr(bb_start);
		cc.SetInsertPoint(bb_start);
	}


	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(Names... names) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough or too many names specified");
		// create all parameter wrapper objects in a tuple
		std::tuple<wrapper_type<F,Args>...> ret { wrapper_type<F,Args>(cc, names)... };
		// get argument value and put it in wrapper object
		std::apply(
			[&](auto &&...args){
				llvm::Function *fn = cc.GetInsertBlock()->getParent();
				llvm::Function::arg_iterator arguments = fn->arg_begin();
				// (tuple_at_0 = (llvm::Value*)args++), (tuple_at_1 = (llvm::Value*)args++), ... ;
				((args = (llvm::Value*)arguments++), ...);
			},
			ret
		);
		return ret;
	}

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
		llvm::orc::ThreadSafeModule tsm(std::move(M), std::move(context));
		cantFail(jit.J->addIRModule(std::move(tsm)));
		// Look up the JIT'd function
		llvm::JITEvaluatedSymbol SumSym = cantFail(jit.J->lookup(name));
		// get function ptr
		return (func_type)SumSym.getAddress();
	}

	operator const llvm::IRBuilder<>&() const { return cc; }
	operator       llvm::IRBuilder<>&()       { return cc; }

	void optimize(int optLevel){
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
		std::string str;
		llvm::raw_string_ostream os(str);
		bool failed = llvm::verifyFunction(*func, &os);
		if(failed){
			fprintf(stderr, "\nfunction verifier:\n%s\n", os.str().c_str());
		}
		return !failed;
	}
};


template<typename R, typename ...Args>
struct InternalFunction<runtimellvmjit,R(*)(Args...)>{
	using F = ::llvm::IRBuilder<>;
	using func_type = R (*)(Args...);
	using return_type = R;

	llvm::IRBuilder<> &cc;
	const char *name;
	llvm::Function *func;

	//TODO: make private, Function::addFunction should be used
	InternalFunction(runtimellvmjit &jit, llvm::IRBuilder<> &cc, const char *name, llvm::Function *func)
		: cc(cc), name(name), func(func) {}
	InternalFunction(const InternalFunction &other) : cc(other.cc), name(other.name), func(other.func) {}


	template<typename ...Names>
	std::tuple<wrapper_type<F,Args>...> getArguments(Names... names) {
		static_assert(sizeof...(Args) == sizeof...(Names), "not enough or too many names specified");
		// create all parameter wrapper objects in a tuple
		std::tuple<wrapper_type<F,Args>...> ret { wrapper_type<F,Args>(cc, names)... };
		// get argument value and put it in wrapper object
		std::apply(
			[&](auto &&...args){
				llvm::Function *fn = cc.GetInsertBlock()->getParent();
				llvm::Function::arg_iterator arguments = fn->arg_begin();
				// (tuple_at_0 = (llvm::Value*)args++), (tuple_at_1 = (llvm::Value*)args++), ... ;
				((args = (llvm::Value*)arguments++), ...);
			},
			ret
		);
		return ret;
	}

	operator const llvm::IRBuilder<>&() const { return cc; }
	operator       llvm::IRBuilder<>&()       { return cc; }
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
