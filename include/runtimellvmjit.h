#ifndef RUNTIMELLVMJIT_H_
#define RUNTIMELLVMJIT_H_

// use new code by default for now
#define LLVM6 0


#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#include <llvm/IR/Mangler.h>
#if !LLVM6
#include <llvm/ExecutionEngine/JITEventListener.h>
#endif

#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

#include <llvm/IR/Verifier.h>


class runtimellvmjit {
public:
	llvm::LLVMContext context;
	std::unique_ptr<llvm::Module> module{nullptr}; // set in createFunction
	// all functions in module
	std::vector<llvm::Function*> functions;
	// latest function, which is currently generated
	llvm::Function *jit_func;

private:
	std::unique_ptr<llvm::TargetMachine> tm;
	const llvm::DataLayout dl;
	llvm::orc::RTDyldObjectLinkingLayer ObjectLayer;
	llvm::orc::IRCompileLayer<decltype(ObjectLayer), llvm::orc::SimpleCompiler> CompileLayer;

#if LLVM6
	using ModuleHandle = decltype(CompileLayer)::ModuleHandleT;
	//TODO: means probably that we can only for one query, fine for now, extend later
	ModuleHandle h;
#else
	llvm::orc::ExecutionSession ES;
	std::shared_ptr<llvm::orc::SymbolResolver> resolver;
	std::vector<llvm::orc::VModuleKey> moduleKeys;
	llvm::JITEventListener *gdbEventListener;
#endif

	// optimization level
	int optLevel=3;

	std::unordered_map<std::string,llvm::Function*> externalFunctions;

public:
	static void initTarget(){
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
	}

	runtimellvmjit()
#if LLVM6
		: tm(llvm::EngineBuilder().selectTarget()), dl(tm->createDataLayout()),
			ObjectLayer([]() { return std::make_shared<llvm::SectionMemoryManager>(); }),
		CompileLayer(ObjectLayer, llvm::orc::SimpleCompiler(*tm))
#else
		: tm(llvm::EngineBuilder().selectTarget())
		, dl(tm->createDataLayout())
		, ObjectLayer(ES,
			[this](llvm::orc::VModuleKey) {
					return decltype(ObjectLayer)::Resources{
							std::make_shared<llvm::SectionMemoryManager>(),
							resolver
					};
		  	}, [this](llvm::orc::VModuleKey /*key*/, const llvm::object::ObjectFile &obj, const llvm::RuntimeDyld::LoadedObjectInfo &info){
				// lambda called for objectLoaded
				// pass to gdb event listener as objectEmitted
				gdbEventListener->NotifyObjectEmitted(obj, info);
			}
		  )
		, CompileLayer(ObjectLayer, llvm::orc::SimpleCompiler(*tm))
		, resolver(llvm::orc::createLegacyLookupResolver(
			ES,
			[this](const std::string &name) -> llvm::JITSymbol {
				if(auto sym = CompileLayer.findSymbol(name, false)){
					return sym;
				}else if(auto err = sym.takeError()){
					return std::move(err);
				}
				if(auto symaddr = llvm::RTDyldMemoryManager::getSymbolAddressInProcess(name)){
					return llvm::JITSymbol(symaddr, llvm::JITSymbolFlags::Exported);
				}
				//FIXME: error handling when symbol not found
				//return nullptr;
				// hard crash
				__builtin_trap(); // symbol not found, did you forget to compile with "-rdynamic"?
			},
			[](llvm::Error err){
				cantFail(std::move(err), "lookupFlags failed");
			}
		  ))
#endif
	{
		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

#if !LLVM6
		gdbEventListener = llvm::JITEventListener::createGDBRegistrationListener();
#endif
	}

	void reset(){
		module.reset(new llvm::Module("blamodule", context)); // useless module name
		module->setDataLayout(dl);
		functions.clear();
		externalFunctions.clear();

#if 0
		//DEBUG - BEGIN
		llvm::Type *int64_type = llvm::Type::getInt64Ty(context);
		unsigned alignment = dl.getPrefTypeAlignment(int64_type);
		printf("int64 alignment: %u\n", alignment);
		//DEBUG - END
#endif
	}
	void createFunction(llvm::FunctionType *jit_func_type, const char *name){
		jit_func = llvm::Function::Create(jit_func_type, llvm::Function::ExternalLinkage, name, module.get());
		functions.push_back(jit_func);
	}

#if 0
	//DEBUG - BEGIN
	const llvm::StructLayout *getStructLayout(llvm::StructType *type) const {
		return dl.getStructLayout(type);
	}
	//DEBUG - END
#endif

	llvm::Function *addExternalFunction(const char *name, llvm::FunctionType *func_type){
		llvm::Function *func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name, module.get());
		func->setCallingConv(llvm::CallingConv::C);
		externalFunctions.emplace(name, func);
		return func;
	}
	llvm::Function *getExternalFunction(const char *name) const {
		auto it = externalFunctions.find(name);
		if(it != externalFunctions.end()){
			return it->second;
		}else{
			return nullptr;
		}
	}

	bool verifyFunction(llvm::Function *jit_func){
		std::string str;
		llvm::raw_string_ostream os(str);
		bool failed = llvm::verifyFunction(*jit_func, &os);
		if(failed){
			fprintf(stderr, "\nfunction verifier:\n%s\n", str.c_str());
		}
		return !failed;
	}
	bool verifyFunctions(){
		bool ret=true;
		for(llvm::Function *f : functions){
			ret = ret && verifyFunction(f);
		}
		return ret;
	}

	void setOptLevel(int optLevel){
		this->optLevel = optLevel;
	}

	void optimize(){
		if(optLevel > 0){
#if 1
			llvm::PassManagerBuilder pm_builder;
			pm_builder.OptLevel = optLevel;
			pm_builder.SizeLevel = 0;
			pm_builder.LoopVectorize = true;
			pm_builder.SLPVectorize = true;

			llvm::legacy::FunctionPassManager function_pm(module.get());
			llvm::legacy::PassManager module_pm;
			pm_builder.populateFunctionPassManager(function_pm);
			pm_builder.populateModulePassManager(module_pm);

			function_pm.doInitialization();
			for(llvm::Function *f : functions){
				function_pm.run(*f);
			}
			module_pm.run(*module);
#else
			//FIXME: legacy namespace implies that there is a newer way to do it
			llvm::legacy::FunctionPassManager function_pm(module.get());
			// Do simple "peephole" optimizations and bit-twiddling optzns.
			function_pm.add(llvm::createInstructionCombiningPass());
			// Reassociate expressions.
			function_pm.add(llvm::createReassociatePass());
			// Eliminate Common SubExpressions.
			function_pm.add(llvm::createGVNPass());
			// Simplify the control flow graph (deleting unreachable blocks, etc).
			function_pm.add(llvm::createCFGSimplificationPass());
			// init
			function_pm.doInitialization();
			//FIXME: do all the initialization at start, not per call

			// Optimize the function.
			function_pm.run(*jit_func);
#endif
		}
	}

	void finalize(){
#if LLVM6
		// Build our symbol resolver:
		// Lambda 1: Look back into the JIT itself to find symbols that are part of
		//           the same "logical dylib".
		// Lambda 2: Search for external symbols in the host process.
		auto Resolver = llvm::orc::createLambdaResolver(
			[&](const std::string &Name) {
				if(auto Sym = CompileLayer.findSymbol(Name, false)){
					return Sym;
				}
				return llvm::JITSymbol(nullptr);
			},
			[](const std::string &Name) {
				if(auto SymAddr = llvm::RTDyldMemoryManager::getSymbolAddressInProcess(Name)){
					return llvm::JITSymbol(SymAddr, llvm::JITSymbolFlags::Exported);
				}
				return llvm::JITSymbol(nullptr);
			}
		);

		// Add the set to the JIT with the resolver we created above and a newly
		// created SectionMemoryManager.
		//ATTENTION: moves module, not usable afterwards, create new with createFunction
		h = cantFail(CompileLayer.addModule(std::move(module), std::move(Resolver)));
#else
		auto K = ES.allocateVModule();
		cantFail(CompileLayer.addModule(K, std::move(module)));
		moduleKeys.push_back(K);
#endif
	}
	void free(){
#if LLVM6
		cantFail(CompileLayer.removeModule(h));
#else
		for(auto K : moduleKeys){
			cantFail(CompileLayer.removeModule(K));
		}
		moduleKeys.clear();
#endif
	}

	llvm::JITSymbol findSymbol(const std::string name) {
		std::string mangledName;
		{
			llvm::raw_string_ostream mangledNameStream(mangledName);
			llvm::Mangler::getNameWithPrefix(mangledNameStream, name, dl);
		}
		for(auto h : llvm::make_range(moduleKeys.rbegin(), moduleKeys.rend())){
			if(auto sym = CompileLayer.findSymbolIn(h, mangledName, true)){
				return sym;
			}
		}
		__builtin_trap(); //FIXME: hard crash
	}

	llvm::JITTargetAddress getSymbolAddress(const std::string name) {
		return cantFail(findSymbol(name).getAddress());
	}

	// print IR to file
	void print(const char *filename) {
		std::string str;
		llvm::raw_string_ostream os(str);
		module->print(os, nullptr);

		std::ofstream of(filename);
		of << os.str();
	}
};

#endif
