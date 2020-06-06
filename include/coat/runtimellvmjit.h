#ifndef COAT_RUNTIMELLVMJIT_H_
#define COAT_RUNTIMELLVMJIT_H_


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

#include <llvm/ExecutionEngine/JITEventListener.h>

#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

#include <llvm/IR/Verifier.h>

#ifndef NDEBUG
#  include <iostream>
#endif


namespace coat {

class runtimellvmjit {
public:
	llvm::LLVMContext context;
	std::unique_ptr<llvm::Module> module{nullptr}; // set in createFunction
	// all functions in module
	std::vector<llvm::Function*> functions;

private:
	std::unique_ptr<llvm::TargetMachine> tm;
	const llvm::DataLayout dl;
	llvm::orc::RTDyldObjectLinkingLayer ObjectLayer;
	llvm::orc::IRCompileLayer<decltype(ObjectLayer), llvm::orc::SimpleCompiler> CompileLayer;

	llvm::orc::ExecutionSession ES;
	std::shared_ptr<llvm::orc::SymbolResolver> resolver;
	std::vector<llvm::orc::VModuleKey> moduleKeys;
	llvm::JITEventListener *gdbEventListener;

	// optimization level
	int optLevel=3;

	static std::unique_ptr<llvm::TargetMachine> march_native(){
		llvm::orc::JITTargetMachineBuilder TMD = cantFail(llvm::orc::JITTargetMachineBuilder::detectHost());
		
		//FIXME: this is done in detectHost in newer LLVM versions
		// enable all CPU features
		llvm::SubtargetFeatures stf;
		llvm::StringMap<bool> featuremap;
		llvm::sys::getHostCPUFeatures(featuremap);
		for(const auto &feature : featuremap){
			stf.AddFeature(feature.first(), feature.second);
		}
		TMD.setCPU(llvm::sys::getHostCPUName());
		TMD.addFeatures(stf.getFeatures());

		return cantFail(TMD.createTargetMachine());
	}


public:
	static void initTarget(){
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
	}

	runtimellvmjit()
		: tm(march_native())
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
				__builtin_trap(); // symbol not found
			},
			[](llvm::Error err){
				cantFail(std::move(err), "lookupFlags failed");
			}
		  ))
	{
		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

		gdbEventListener = llvm::JITEventListener::createGDBRegistrationListener();

#ifndef NDEBUG
		std::cout << "Triple: " << llvm::sys::getProcessTriple()
				<< "\nCPU: " << tm->getTargetCPU().str()
				<< "\nFeatures: " << tm->getTargetFeatureString().str() << '\n';
#endif
	}

	void reset(){
		module.reset(new llvm::Module("blamodule", context)); // useless module name
		module->setDataLayout(dl);
		functions.clear();

#if 0
		//DEBUG - BEGIN
		llvm::Type *int64_type = llvm::Type::getInt64Ty(context);
		unsigned alignment = dl.getPrefTypeAlignment(int64_type);
		printf("int64 alignment: %u\n", alignment);
		//DEBUG - END
#endif
	}
	llvm::Function *createFunction(llvm::FunctionType *jit_func_type, const char *name, llvm::GlobalValue::LinkageTypes linkage=llvm::Function::ExternalLinkage){
		llvm::Function *func = llvm::Function::Create(jit_func_type, linkage, name, module.get());
		functions.push_back(func);
		return func;
	}

#if 0
	//DEBUG - BEGIN
	const llvm::StructLayout *getStructLayout(llvm::StructType *type) const {
		return dl.getStructLayout(type);
	}
	//DEBUG - END
#endif

	bool verifyFunction(llvm::Function *jit_func){
		std::string str;
		llvm::raw_string_ostream os(str);
		bool failed = llvm::verifyFunction(*jit_func, &os);
		if(failed){
			fprintf(stderr, "\nfunction verifier:\n%s\n", os.str().c_str());
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
	int getOptLevel() const {
		return optLevel;
	}

	void optimize(){
		if(optLevel > 0){
			llvm::PassManagerBuilder pm_builder;
			pm_builder.OptLevel = optLevel;
			pm_builder.SizeLevel = 0;
			//pm_builder.Inliner = llvm::createAlwaysInlinerLegacyPass();
			pm_builder.Inliner = llvm::createFunctionInliningPass(optLevel, 0, false);
			pm_builder.LoopVectorize = true;
			pm_builder.SLPVectorize = true;

			//pm_builder.VerifyInput = true;
			pm_builder.VerifyOutput = true;

			llvm::legacy::FunctionPassManager function_pm(module.get());
			llvm::legacy::PassManager module_pm;
			pm_builder.populateFunctionPassManager(function_pm);
			pm_builder.populateModulePassManager(module_pm);

			function_pm.doInitialization();
			for(llvm::Function *f : functions){
				function_pm.run(*f);
			}
			module_pm.run(*module);
		}
	}

	void finalize(){
		auto K = ES.allocateVModule();
		cantFail(CompileLayer.addModule(K, std::move(module)));
		moduleKeys.push_back(K);
	}
	void free(){
		for(auto K : moduleKeys){
			cantFail(CompileLayer.removeModule(K));
		}
		moduleKeys.clear();
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
};

} // namespace

#endif
