#ifndef COAT_LLVMJIT_STRUCT_H_
#define COAT_LLVMJIT_STRUCT_H_


namespace coat {

template<typename T>
struct Struct<::llvm::IRBuilder<>,T> {
	using F = ::llvm::IRBuilder<>;

	static_assert(std::is_standard_layout_v<T>, "wrapped class needs to have standard layout");

	llvm::IRBuilder<> &builder;
	llvm::Value *memreg;

	llvm::Value *load() const { return builder.CreateLoad(memreg, "load"); }
	void store(llvm::Value *v) { builder.CreateStore(v, memreg); }
	llvm::Type *type() const { return ((llvm::PointerType*)memreg->getType())->getElementType(); }

	Struct(llvm::IRBuilder<> &builder, const char *name="") : builder(builder) {
		// create struct type in LLVM IR from T::types tuple
		llvm::StructType *struct_type = getLLVMStructType<T>(builder.getContext());
		llvm::Type *struct_ptr_type = llvm::PointerType::get(struct_type, 0);
		// allocate space on stack to store pointer to struct
		memreg = builder.CreateAlloca(struct_ptr_type, nullptr, name);
	}

	// load base pointer
	Struct &operator=(T *ptr){
		// store pointer, but how to get immediate/constant with correct LLVM type?
		store(
			llvm::ConstantExpr::getIntToPtr(
				llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), (uint64_t)ptr),
				((llvm::PointerType*)memreg->getType())->getElementType()
			)
		);
		return *this;
	}
	//FIXME: takes any type
	Struct &operator=(llvm::Value *val){ store( val ); return *this; }

	// pre-increment
	Struct &operator++(){
		store( builder.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), 1)) );
		return *this;
	}
	// pre-decrement
	Struct &operator--(){
		store( builder.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), -1)) );
		return *this;
	}

	Struct operator+ (int amount) const {
		Struct res(builder);
		res.store( builder.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), amount)) );
		return res;
	}

	template<int I>
	Ref<F,reg_type<F,std::tuple_element_t<I, typename T::types>>> get_reference(){
		return { builder, builder.CreateStructGEP(load(), I) };
	}

	template<int I>
	wrapper_type<F,std::tuple_element_t<I, typename T::types>> get_value(){
		wrapper_type<F,std::tuple_element_t<I, typename T::types>> ret(builder);
		llvm::Value *member_addr = builder.CreateStructGEP(load(), I);
		if constexpr(std::is_arithmetic_v<std::remove_pointer_t<std::tuple_element_t<I, typename T::types>>>){
			llvm::Value *member = builder.CreateLoad(member_addr, "memberload");
			ret.store(member);
		}else{
			if constexpr(std::is_pointer_v<std::tuple_element_t<I, typename T::types>>){
				// pointer to struct, load pointer
				llvm::Value *member = builder.CreateLoad(member_addr, "memberload");
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
