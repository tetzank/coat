#ifndef COAT_LLVMJIT_PTR_H_
#define COAT_LLVMJIT_PTR_H_

#include "ValueBase.h"
//#include "Ref.h"


namespace coat {

template<class T>
struct Ptr<LLVMBuilders,T> {
	using F = LLVMBuilders;
	using value_type = typename T::value_type;
	using value_base_type = ValueBase<F>;
	using mem_type = Ref<F,T>;

	static_assert(std::is_base_of_v<value_base_type,T>, "pointer type only of register wrappers");

	LLVMBuilders &cc;
	llvm::Value *memreg;
#ifdef LLVMJIT_DEBUG
	//FIXME: try to minimize
	llvm::DILocalVariable *dbgvar;
	llvm::IntrinsicInst *dbgdecl;
#endif

	llvm::Value *load() const { return cc.ir.CreateLoad(memreg, "load"); }
	void store(llvm::Value *v) { cc.ir.CreateStore(v, memreg); }
	llvm::Type *type() const { return ((llvm::PointerType*)memreg->getType())->getElementType(); }

	Ptr(F &cc, const char *name=""
#ifdef LLVMJIT_DEBUG
			,bool isParameter=false,
			const char * /*file*/=__builtin_FILE(),
			int line=__builtin_LINE()
#endif
	) : cc(cc) {
		// llvm IR has no types for unsigned/signed integers
		switch(sizeof(value_type)){
			case 1: memreg = allocateStackVariable(cc.ir, llvm::Type::getInt8PtrTy (cc.ir.getContext()), name); break;
			case 2: memreg = allocateStackVariable(cc.ir, llvm::Type::getInt16PtrTy(cc.ir.getContext()), name); break;
			case 4: memreg = allocateStackVariable(cc.ir, llvm::Type::getInt32PtrTy(cc.ir.getContext()), name); break;
			case 8: memreg = allocateStackVariable(cc.ir, llvm::Type::getInt64PtrTy(cc.ir.getContext()), name); break;
		}
#ifdef LLVMJIT_DEBUG
		// debug information
		//TODO: file?
		if(isParameter){
			//TODO: param number
			dbgvar = cc.dbg.createParameterVariable(cc.debugScope, name, 0, cc.debugScope->getFile(), line, getDebugType<value_type*>(cc.dbg, cc.debugScope), true); //TODO: why alwaysPreserve=true?
		}else{
			dbgvar = cc.dbg.createAutoVariable(cc.debugScope, name, cc.debugScope->getFile(), line, getDebugType<value_type*>(cc.dbg, cc.debugScope));
		}
		dbgdecl = static_cast<llvm::IntrinsicInst*>(cc.dbg.insertDeclare(memreg, dbgvar, cc.dbg.createExpression(), llvm::DebugLoc::get(line, 0, cc.debugScope), cc.ir.GetInsertBlock()));
#endif
	}
	Ptr(F &cc, value_type *val, const char *name="") : Ptr(cc, name) {
		*this = val;
	}
	Ptr(F &cc, const value_type *val, const char *name="") : Ptr(cc, name) {
		*this = const_cast<value_type*>(val);
	}
	// real copy requires new stack memory and copy of content
	Ptr(const Ptr &other) : Ptr(other.cc) {
		*this = other;
	}
	// move, just take the stack memory
	Ptr(const Ptr &&other) : cc(other.cc), memreg(other.memreg) {}

	//FIXME: takes any type
	Ptr &operator=(llvm::Value *val){ store( val ); return *this; }

	Ptr &operator=(value_type *value){
		llvm::Constant *int_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), (uint64_t)value);
		store( cc.ir.CreateIntToPtr(int_val, type()) );
		return *this;
	}
	Ptr &operator=(const Ptr &other){ store( other.load() ); return *this; }

	void setName(const char *name){
		memreg->setName(name);
#ifdef LLVMJIT_DEBUG
		llvm::DILocalVariable *di_var;
		if(dbgvar->isParameter()){
			di_var = cc.dbg.createParameterVariable(dbgvar->getScope(), name, dbgvar->getArg(), dbgvar->getFile(), dbgvar->getLine(), dbgvar->getType(), true); //TODO: why alwaysPreserve=true?
		}else{
			di_var = cc.dbg.createAutoVariable(dbgvar->getScope(), name, dbgvar->getFile(), dbgvar->getLine(), dbgvar->getType());
		}
		dbgdecl->setArgOperand(1, llvm::MetadataAsValue::get(cc.ir.getContext(), di_var)); //FIXME: memleak? what happens with old operand?
#endif
	}

	// dereference
	mem_type operator*(){
		return { cc, cc.ir.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), 0)) };
	}
	// indexing with variable
	mem_type operator[](const value_base_type &idx){
		return { cc, cc.ir.CreateGEP(load(), idx.load()) };
	}
	// indexing with constant -> use offset
	mem_type operator[](size_t idx){
		return { cc, cc.ir.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), idx)) };
	}
	
	Ptr operator+(const D2<value_base_type> &other) const {
#ifdef LLVMJIT_DEBUG
		Ptr res(cc, "temp", false, other.file, other.line);
#else
		Ptr res(cc);
#endif
		DL2;
		res.store( cc.ir.CreateGEP(load(), OP2.load()) );
		return res;
	}
	Ptr operator+(D2<size_t> other) const {
#ifdef LLVMJIT_DEBUG
		Ptr res(cc, "temp", false, other.file, other.line);
#else
		Ptr res(cc);
#endif
		DL2;
		res.store( cc.ir.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), OP2)) );
		return res;
	}

	Ptr &operator+=(const value_base_type &value){
		store( cc.ir.CreateGEP(load(), value.load()) );
		return *this;
	}
	Ptr &operator+=(int amount){
		store( cc.ir.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), amount)) );
		return *this;
	}
	Ptr &operator-=(int amount){
		store( cc.ir.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), -amount)) );
		return *this;
	}

	// like "+=" without pointer arithmetic
	Ptr &addByteOffset(const value_base_type &value){ //TODO: any integer value should be possible as operand
		llvm::Value *int_reg = cc.ir.CreatePtrToInt(load(), llvm::Type::getInt64Ty(cc.ir.getContext()));
		llvm::Value *int_value = cc.ir.CreatePtrToInt(value.load(), llvm::Type::getInt64Ty(cc.ir.getContext()));
		llvm::Value *int_sum = cc.ir.CreateAdd(int_reg, int_value);
		store( cc.ir.CreateIntToPtr(int_sum, type()) );
		return *this;
	}

	// operators creating temporary
	Value<F,size_t> operator- (const Ptr &other) const {
		Value<F,size_t> ret(cc, "ret");
		llvm::Value *int_reg = cc.ir.CreatePtrToInt(load(), llvm::Type::getInt64Ty(cc.ir.getContext()));
		llvm::Value *int_other = cc.ir.CreatePtrToInt(other.load(), llvm::Type::getInt64Ty(cc.ir.getContext()));
		llvm::Value *bytes = cc.ir.CreateSub(int_reg, int_other);
		// compilers do arithmetic shift...
		llvm::Value *elements = cc.ir.CreateAShr(bytes, clog2(sizeof(value_type)), "", true);
		ret.store(elements);
		return ret;
	}

	// pre-increment, post-increment not provided as it creates temporary
	Ptr &operator++(){
		store( cc.ir.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), 1)) );
		return *this;
	}
	// pre-decrement
	Ptr &operator--(){
		store( cc.ir.CreateGEP(load(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(cc.ir.getContext()), -1)) );
		return *this;
	}

	// comparisons
	Condition<F> operator==(const Ptr &other) const { return {cc, memreg, other.memreg, ConditionFlag::e};  }
	Condition<F> operator!=(const Ptr &other) const { return {cc, memreg, other.memreg, ConditionFlag::ne}; }
};


template<typename dest_type, typename src_type>
Ptr<LLVMBuilders,Value<LLVMBuilders,std::remove_pointer_t<dest_type>>>
cast(const Ptr<LLVMBuilders,Value<LLVMBuilders,src_type>> &src){
	static_assert(std::is_pointer_v<dest_type>, "a pointer type can only be casted to another pointer type");

	// create new pointer
	Ptr<LLVMBuilders,Value<LLVMBuilders,std::remove_pointer_t<dest_type>>> res(src.cc);
	// cast between pointer types
	res.store(
		src.cc.ir.CreateBitCast(
			src.load(),
			res.type()
		)
	);
	// return new pointer
	return res;
}

} // namespace

#endif
