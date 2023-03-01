#ifndef COAT_RUNTIMELLVMJIT_H_
#define COAT_RUNTIMELLVMJIT_H_

#include <utility> // tuple_size, tuple_element
#include "constexpr_helper.h" // should_not_be_reached

#include <llvm/Support/TargetSelect.h> // InitializeNativeTarget

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ObjectTransformLayer.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>


namespace coat {


template<typename T>
inline llvm::Type *getLLVMType(llvm::LLVMContext &ctx);

template<class Tuple, std::size_t... I>
inline llvm::StructType *getLLVMStructTypeImpl(llvm::LLVMContext &ctx, std::index_sequence<I...>){
	return llvm::StructType::get(
		getLLVMType<std::tuple_element_t<I, Tuple>>(ctx)...
	);
}
template<typename T>
inline llvm::StructType *getLLVMStructType(llvm::LLVMContext &ctx){
	constexpr size_t N = std::tuple_size_v<typename T::types>;
	// N-1 as macros add trailing void to types tuple
	return getLLVMStructTypeImpl<typename T::types>(ctx, std::make_index_sequence<N-1>{});
}


//TODO: a static member function in Value<T>, Ptr<T>, Struct<T> would be better
//      the ::type() is there already, but cannot be static
template<typename T>
inline llvm::Type *getLLVMType(llvm::LLVMContext &ctx){
	if constexpr(std::is_pointer_v<T>){
		using base_type = std::remove_cv_t<std::remove_pointer_t<T>>;
		if constexpr(std::is_integral_v<base_type>){
			if constexpr(std::is_same_v<base_type,char> || std::is_same_v<base_type,unsigned char>){
				return llvm::Type::getInt8PtrTy(ctx);
			}else if constexpr(std::is_same_v<base_type,short> || std::is_same_v<base_type,unsigned short>){
				return llvm::Type::getInt16PtrTy(ctx);
			}else if constexpr(std::is_same_v<base_type,int> || std::is_same_v<base_type,unsigned>){
				return llvm::Type::getInt32PtrTy(ctx);
			}else if constexpr(std::is_same_v<base_type,long> || std::is_same_v<base_type,unsigned long>){
				return llvm::Type::getInt64PtrTy(ctx);
			}else{
				static_assert(should_not_be_reached<T>, "type not supported");
			}
		}else{
			return llvm::PointerType::get(getLLVMStructType<base_type>(ctx), 0);
		}
	}else if constexpr(std::is_array_v<T>){
		static_assert(std::rank_v<T> == 1, "multidimensional arrays are not supported");
		return llvm::ArrayType::get(getLLVMType<std::remove_extent_t<T>>(ctx), std::extent_v<T>);
	}else{
		if constexpr(std::is_same_v<T,void>){
			return llvm::Type::getVoidTy(ctx);
		}else if constexpr(std::is_same_v<T,char> || std::is_same_v<T,unsigned char>){
			return llvm::Type::getInt8Ty(ctx);
		}else if constexpr(std::is_same_v<T,short> || std::is_same_v<T,unsigned short>){
			return llvm::Type::getInt16Ty(ctx);
		}else if constexpr(std::is_same_v<T,int> || std::is_same_v<T,unsigned>){
			return llvm::Type::getInt32Ty(ctx);
		}else if constexpr(std::is_same_v<T,long> || std::is_same_v<T,unsigned long>){
			return llvm::Type::getInt64Ty(ctx);
		}else{
			//HACK: assumes that this is a structure
			return getLLVMStructType<T>(ctx);
			//static_assert(should_not_be_reached<T>, "type not supported");
		}
	}
	//return nullptr;
	__builtin_trap(); // must not be reached, crashes if it still is
}



class runtimellvmjit {
//private: //FIXME
public:
	std::unique_ptr<llvm::orc::LLJIT> J;

public:
	static void initTarget(){
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
	}

	//TODO: add configuration parameters/flags
	// like dump_assembly, gdb_support, perf_support, opt_level
	runtimellvmjit(){
		//J = ExitOnErr(
		J = cantFail(
			llvm::orc::LLJITBuilder()
				.setJITTargetMachineBuilder(cantFail(llvm::orc::JITTargetMachineBuilder::detectHost()))
				.setCompileFunctionCreator([&](llvm::orc::JITTargetMachineBuilder JTMB)
					-> llvm::Expected<std::unique_ptr<llvm::orc::IRCompileLayer::IRCompiler>>
				{
					auto TM = JTMB.createTargetMachine();
					if (!TM) return TM.takeError();
					return std::make_unique<llvm::orc::TMOwningSimpleCompiler>(std::move(*TM));
				})
				.setObjectLinkingLayerCreator([&](llvm::orc::ExecutionSession &ES, const llvm::Triple &/*TT*/) {
					auto GetMemMgr = []() {
						return std::make_unique<llvm::SectionMemoryManager>();
					};
					auto ObjLinkingLayer = std::make_unique<llvm::orc::RTDyldObjectLinkingLayer>(
						ES, std::move(GetMemMgr)
					);
					ObjLinkingLayer->registerJITEventListener(
						*llvm::JITEventListener::createGDBRegistrationListener()
					);
					if(const char *jitdump=getenv("COAT_JITDUMP"); jitdump && jitdump[0]=='1'){
						llvm::JITEventListener *perfListener = llvm::JITEventListener::createPerfJITEventListener();
						// is nullptr without LLVM_USE_PERF when compiling LLVM
						assert(perfListener);
						ObjLinkingLayer->registerJITEventListener(*perfListener);
					}
					return ObjLinkingLayer;
				})
				.create()
		);
		if(const char *objdump=getenv("COAT_OBJDUMP"); objdump && objdump[0]=='1'){
			// dumps object files to current working directory (directory can be changed by parameter)
			J->getObjTransformLayer().setTransform(llvm::orc::DumpObjects());
		}
	}

	template<typename FnPtr>
	Function<runtimellvmjit,FnPtr> createFunction(
		const char *funcName="func"
#ifdef LLVMJIT_DEBUG
		,const char *srcfile=__builtin_FILE(), int srcline=__builtin_LINE()
#endif
	){
#ifdef LLVMJIT_DEBUG
		return Function<runtimellvmjit,FnPtr>(*this, funcName, srcfile, srcline);
#else
		return Function<runtimellvmjit,FnPtr>(*this, funcName);
#endif
	}
};

} // namespace

#endif
