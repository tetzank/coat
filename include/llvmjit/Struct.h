#ifndef COAT_LLVMJIT_STRUCT_H_
#define COAT_LLVMJIT_STRUCT_H_


namespace coat {

template<typename T>
struct Struct<::llvm::IRBuilder<>,T> {
	using F = ::llvm::IRBuilder<>;

	static_assert(std::is_standard_layout_v<T>, "wrapped class needs to have standard layout");

	llvm::IRBuilder<> &cc;
	llvm::Value *memreg;

	llvm::Value *load() const { return cc.CreateLoad(memreg, "load"); }
	void store(llvm::Value *v) { cc.CreateStore(v, memreg); }
	llvm::Type *type() const { return ((llvm::PointerType*)memreg->getType())->getElementType(); }

	Struct(llvm::IRBuilder<> &cc, const char *name="") : cc(cc) {
		// create struct type in LLVM IR from T::types tuple
		llvm::StructType *struct_type = getLLVMStructType<T>(cc.getContext());
		llvm::Type *struct_ptr_type = llvm::PointerType::get(struct_type, 0);
		// allocate space on stack to store pointer to struct
		memreg = cc.CreateAlloca(struct_ptr_type, nullptr, name);
	}

	// load base pointer
	Struct &operator=(T *ptr){
		// store pointer, but how to get immediate/constant with correct LLVM type?
		store(
			llvm::ConstantExpr::getIntToPtr(
				llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.getContext()), (uint64_t)ptr),
				((llvm::PointerType*)memreg->getType())->getElementType()
			)
		);
		return *this;
	}
	//FIXME: takes any type
	Struct &operator=(llvm::Value *val){ store( val ); return *this; }

	// pre-increment
	Struct &operator++(){
		store( cc.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.getContext()), 1)) );
		return *this;
	}
	// pre-decrement
	Struct &operator--(){
		store( cc.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.getContext()), -1)) );
		return *this;
	}

	Struct operator+ (int amount) const {
		Struct res(cc);
		res.store( cc.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.getContext()), amount)) );
		return res;
	}

	template<int I>
	Ref<F,reg_type<F,std::tuple_element_t<I, typename T::types>>> get_reference(){
		return { cc, cc.CreateStructGEP(load(), I) };
	}

	template<int I>
	wrapper_type<F,std::tuple_element_t<I, typename T::types>> get_value(){
		wrapper_type<F,std::tuple_element_t<I, typename T::types>> ret(cc);
		llvm::Value *member_addr = cc.CreateStructGEP(load(), I);
		if constexpr(std::is_arithmetic_v<std::remove_pointer_t<std::tuple_element_t<I, typename T::types>>>){
			llvm::Value *member = cc.CreateLoad(member_addr, "memberload");
			ret.store(member);
		}else{
			if constexpr(std::is_pointer_v<std::tuple_element_t<I, typename T::types>>){
				// pointer to struct, load pointer
				llvm::Value *member = cc.CreateLoad(member_addr, "memberload");
				ret.store(member);
			}else{
				// nested struct, just move "offset"
				ret.store( member_addr );
			}
		}
		return ret;
	}
};


} // namespace

#endif
