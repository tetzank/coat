#ifndef COAT_LLVMJIT_DEBUGOPERAND_H_
#define COAT_LLVMJIT_DEBUGOPERAND_H_

#include "../constexpr_helper.h"

#include <llvm/IR/DIBuilder.h>

namespace coat {

template<typename T> llvm::DIType *getDebugType(llvm::DIBuilder &);

template<class Tuple, std::size_t... I>
llvm::DICompositeType *getDebugStructTypeImpl(llvm::DIBuilder &dibuilder, std::index_sequence<I...>){
	//return dibuilder.createStructType(
	//	getDebugType<std::tuple_element_t<I, Tuple>>(dibuilder)...
	//);
	return nullptr; //TODO
}
template<typename T>
llvm::DICompositeType *getDebugStructType(llvm::DIBuilder &dibuilder){
	constexpr size_t N = std::tuple_size_v<typename T::types>;
	// N-1 as macros add trailing void to types tuple
	return getDebugStructTypeImpl<typename T::types>(dibuilder, std::make_index_sequence<N-1>{});
}

template<typename T>
llvm::DIType *getDebugType(llvm::DIBuilder &dibuilder){
	if constexpr(std::is_pointer_v<T>){
		using base_type = std::remove_cv_t<std::remove_pointer_t<T>>;
		if constexpr(std::is_integral_v<base_type>){
			return dibuilder.createPointerType(getDebugType<base_type>(dibuilder), 64);
		}else{
			//TODO: support for pointer to struct
			static_assert(should_not_be_reached<base_type>, "type not supported");
			//return llvm::PointerType::get(getLLVMStructType<base_type>(ctx), 0);
		}
	}else if constexpr(std::is_array_v<T>){
		static_assert(should_not_be_reached<T>, "type not supported"); // FIXME
		//static_assert(std::rank_v<T> == 1, "multidimensional arrays are not supported");
		//return llvm::ArrayType::get(getLLVMType<std::remove_extent_t<T>>(ctx), std::extent_v<T>);
		//return dibuilder.createArrayType(
		//	std::extent_v<T>,
		//	alignof(std::remove_extent_t<T>) * CHAR_BIT,
		//	getDebugType<std::remove_extent_t<T>>(dibuilder),
		//	???
		//);
	}else{
		if constexpr(std::is_same_v<T,void>){
			// no void type in DWARF
			return nullptr; //TODO: double-check if that's correct
		}else if constexpr(std::is_same_v<T,char>){
			return dibuilder.createBasicType("char", 8, llvm::dwarf::DW_ATE_signed_char);
		}else if constexpr(std::is_same_v<T,unsigned char>){
			return dibuilder.createBasicType("unsigned char", 8, llvm::dwarf::DW_ATE_unsigned_char);
		}else if constexpr(std::is_same_v<T,short>){
			return dibuilder.createBasicType("short", 16, llvm::dwarf::DW_ATE_signed);
		}else if constexpr(std::is_same_v<T,unsigned short>){
			return dibuilder.createBasicType("unsigned short", 16, llvm::dwarf::DW_ATE_unsigned);
		}else if constexpr(std::is_same_v<T,int>){
			return dibuilder.createBasicType("int", 32, llvm::dwarf::DW_ATE_signed);
		}else if constexpr(std::is_same_v<T,unsigned>){
			return dibuilder.createBasicType("unsigned int", 32, llvm::dwarf::DW_ATE_unsigned);
		}else if constexpr(std::is_same_v<T,long>){
			return dibuilder.createBasicType("long", 64, llvm::dwarf::DW_ATE_signed);
		}else if constexpr(std::is_same_v<T,unsigned long>){
			return dibuilder.createBasicType("unsigned long", 64, llvm::dwarf::DW_ATE_unsigned);
		}else{
			//HACK: assumes that this is a structure
			//return getLLVMStructType<T>(ctx); //FIXME: support structures too
			static_assert(should_not_be_reached<T>, "type not supported");
		}
	}
	//return nullptr;
	__builtin_trap(); // must not be reached, crashes if it still is
}


//FIXME: copy&paste from asmjit backend, use a common file when it works
// it just carries the source file and line number, additionally to the operand
template<typename T>
struct DebugOperand2 {
	const T &operand;
	const char *file;
	int line;

	DebugOperand2(const T &operand, const char *file=__builtin_FILE(), int line=__builtin_LINE())
		: operand(operand), file(file), line(line) {}
};


template<typename T> using D2 = DebugOperand2<T>;


} // namespace

#endif
