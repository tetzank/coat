#ifndef COAT_FUNCTION_H_
#define COAT_FUNCTION_H_

#include <type_traits>

#ifdef ENABLE_LLVMJIT
#  include "runtimellvmjit.h"
#  include "constexpr_helper.h"
#endif


namespace coat {

//FIXME: forwards
template<typename F, typename T> struct Value;
template<typename F, typename T> struct Ptr;
template<typename F, typename T> struct Struct;


template<typename F, typename T>
using reg_type = std::conditional_t<std::is_pointer_v<T>,
						Ptr<F,Value<F,std::remove_cv_t<std::remove_pointer_t<T>>>>,
						Value<F,std::remove_cv_t<T>>
				>;

template<typename F, typename T>
using wrapper_type = std::conditional_t<std::is_arithmetic_v<std::remove_pointer_t<T>>,
						reg_type<F,T>,
						Struct<F,std::remove_cv_t<std::remove_pointer_t<T>>>
					>;

#ifdef ENABLE_LLVMJIT

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
	return getLLVMStructTypeImpl<typename T::types>(ctx, std::make_index_sequence<N-1>{});
}


//TODO: a static member function in Value<T>, Ptr<T>, VStruct<T> would be better
//      the ::type() is there already, but cannot be static
template<typename T>
inline llvm::Type *getLLVMType(llvm::LLVMContext &ctx){
	if constexpr(std::is_pointer_v<T>){
		using base_type = std::remove_cv_t<std::remove_pointer_t<T>>;
		if constexpr(std::is_integral_v<base_type>){
			if constexpr(std::is_same_v<base_type,short> || std::is_same_v<base_type,unsigned short>){
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
	}else{
		if constexpr(std::is_same_v<T,void>){
			return llvm::Type::getVoidTy(ctx);
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

#endif




template<typename T, typename F>
struct Function;

} // namespace


#ifdef ENABLE_ASMJIT
#  include "asmjit/Function.h"
#endif

#ifdef ENABLE_LLVMJIT
#  include "llvmjit/Function.h"
#endif


#include "Condition.h"
#include "Ref.h"
#include "Value.h"
#include "Ptr.h"
#include "Struct.h"

#endif
