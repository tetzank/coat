#ifndef COAT_LLVMJIT_STRUCT_H_
#define COAT_LLVMJIT_STRUCT_H_


namespace coat {

template<typename T>
struct Struct<LLVMBuilders,T>
	: public std::conditional_t<has_custom_base<T>::value,
								StructBase<Struct<LLVMBuilders,T>>,
								StructBaseEmpty
			>
{
	using F = LLVMBuilders;
	using struct_type = T;

	static_assert(std::is_standard_layout_v<T>, "wrapped class needs to have standard layout");

	LLVMBuilders &cc;
	llvm::Value *memreg;

	llvm::Value *load() const { return cc.ir.CreateLoad(memreg, "load"); }
	void store(llvm::Value *v) { cc.ir.CreateStore(v, memreg); }
	llvm::Type *type() const { return ((llvm::PointerType*)memreg->getType())->getElementType(); }

	Struct(F &cc, const char *name="") : cc(cc) {
		// create struct type in LLVM IR from T::types tuple
		llvm::StructType *struct_type = getLLVMStructType<T>(cc.ir.getContext());
		llvm::Type *struct_ptr_type = llvm::PointerType::get(struct_type, 0);
		// allocate space on stack to store pointer to struct
		memreg = allocateStackVariable(cc.ir, struct_ptr_type, name);
	}
	Struct(F &cc, T *val, const char *name="") : Struct(cc, name) {
		*this = val;
	}
	Struct(F &cc, const T *val, const char *name="") : Struct(cc, name) {
		*this = const_cast<T*>(val);
	}

	// load base pointer
	Struct &operator=(T *ptr){
		// store pointer, but how to get immediate/constant with correct LLVM type?
		store(
			llvm::ConstantExpr::getIntToPtr(
				llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), (uint64_t)ptr),
				((llvm::PointerType*)memreg->getType())->getElementType()
			)
		);
		return *this;
	}
	//FIXME: takes any type
	Struct &operator=(llvm::Value *val){ store( val ); return *this; }

	// pre-increment
	Struct &operator++(){
		store( cc.ir.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), 1)) );
		return *this;
	}
	// pre-decrement
	Struct &operator--(){
		store( cc.ir.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), -1)) );
		return *this;
	}

	Struct operator+ (int amount) const {
		Struct res(cc);
		res.store( cc.ir.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), amount)) );
		return res;
	}

	template<int I>
	Ref<F,reg_type<F,std::tuple_element_t<I, typename T::types>>> get_reference(){
		return { cc, cc.ir.CreateStructGEP(load(), I) };
	}

	template<int I>
	wrapper_type<F,std::tuple_element_t<I, typename T::types>> get_value() const {
		using type = std::tuple_element_t<I, typename T::types>;
		wrapper_type<F,type> ret(cc);
		llvm::Value *member_addr = cc.ir.CreateStructGEP(load(), I);
		if constexpr(std::is_arithmetic_v<std::remove_pointer_t<type>>){
			llvm::Value *member = cc.ir.CreateLoad(member_addr, "memberload");
			ret.store(member);
		}else if constexpr(std::is_pointer_v<type>){
			// pointer to struct, load pointer
			llvm::Value *member = cc.ir.CreateLoad(member_addr, "memberload");
			ret.store(member);
		}else if constexpr(std::is_array_v<type>){
			ret.store( cc.ir.CreateStructGEP(member_addr, 0) );
		}else{
			// nested struct, just move "offset"
			ret.store( member_addr );
		}
		return ret;
	}
};


} // namespace

#endif
