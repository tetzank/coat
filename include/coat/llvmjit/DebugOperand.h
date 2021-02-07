#ifndef COAT_LLVMJIT_DEBUGOPERAND_H_
#define COAT_LLVMJIT_DEBUGOPERAND_H_

#include "../constexpr_helper.h"

#include <llvm/IR/DIBuilder.h>

namespace coat {


#ifdef LLVMJIT_DEBUG

template<typename T> llvm::DIType *getDebugType(llvm::DIBuilder &, llvm::DIScope *);

template<typename T, std::size_t... I>
llvm::DICompositeType *getDebugStructTypeImpl(llvm::DIBuilder &dibuilder, llvm::DIScope *scope, std::index_sequence<I...>){
	const char *name;
	if constexpr(requires {T::name;}){
		name = T::name;
	}else{
		name = "Structure";
	}
	return dibuilder.createStructType(
		//FIXME: line
		scope, name, scope->getFile(), 1 /*line*/,
		sizeof(T)*8 /*sizeInBits*/, alignof(T)*8 /*alignInBits*/,
		llvm::DINode::FlagZero /*flags*/, nullptr /*derivedFrom*/,
		dibuilder.getOrCreateArray(
			{
				dibuilder.createMemberType(
					//FIXME: line
					scope, T::member_names[I], scope->getFile(), 1,
					sizeof(std::tuple_element_t<I, typename T::types>) * 8,
					alignof(std::tuple_element_t<I, typename T::types>) * 8,
					offset_of_v<I, typename T::types> * 8,
					llvm::DINode::FlagZero /*flags*/,
					getDebugType<std::tuple_element_t<I, typename T::types>>(dibuilder, scope)
				)
				...
			}
		)
	);
}
template<typename T>
llvm::DICompositeType *getDebugStructType(llvm::DIBuilder &dibuilder, llvm::DIScope *scope){
	constexpr size_t N = std::tuple_size_v<typename T::types>;
	// N-1 as macros add trailing void to types tuple
	return getDebugStructTypeImpl<T>(dibuilder, scope, std::make_index_sequence<N-1>{});
}

template<typename T>
llvm::DIType *getDebugType(llvm::DIBuilder &dibuilder, llvm::DIScope *scope){
	if constexpr(std::is_pointer_v<T>){
		using base_type = std::remove_cv_t<std::remove_pointer_t<T>>;
		if constexpr(std::is_integral_v<base_type>){
			return dibuilder.createPointerType(getDebugType<base_type>(dibuilder, scope), 64);
		}else{
			// pointer to struct
			return dibuilder.createPointerType(getDebugStructType<base_type>(dibuilder, scope), 64);
		}
	}else if constexpr(std::is_array_v<T>){
		static_assert(should_not_be_reached<T>, "type not supported"); // FIXME
		//static_assert(std::rank_v<T> == 1, "multidimensional arrays are not supported");
		//return llvm::ArrayType::get(getLLVMType<std::remove_extent_t<T>>(ctx), std::extent_v<T>);
		//return dibuilder.createArrayType(
		//	std::extent_v<T>,
		//	alignof(std::remove_extent_t<T>) * CHAR_BIT,
		//	getDebugType<std::remove_extent_t<T>>(dibuilder, scope),
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
			return getDebugStructType<T>(dibuilder, scope);
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
#	define DL2 cc.ir.SetCurrentDebugLocation(llvm::DebugLoc::get(other.line, 0, cc.debugScope));
#	define OP2 other.operand
#else
	// no debugging support
#   define DL2
#	define OP2 other
	template<typename T> using D2 = T;
#endif


} // namespace

#endif
