#ifndef COAT_LLVMJIT_CONDITION_H_
#define COAT_LLVMJIT_CONDITION_H_

#include <variant>

#include <llvm/IR/IRBuilder.h>


namespace coat {

template<>
struct Condition<::llvm::IRBuilder<>> {
	llvm::IRBuilder<> &builder;
	llvm::Value *cmp_result;
	llvm::Value *reg;
	using operand_t = std::variant<llvm::Value*,int>;
	operand_t operand;
	ConditionFlag cond;

	Condition(llvm::IRBuilder<> &builder, llvm::Value *reg, operand_t operand, ConditionFlag cond)
		: builder(builder), reg(reg), operand(operand), cond(cond) {}

	Condition operator!() const {
		ConditionFlag newcond;
		switch(cond){
			case ConditionFlag::e : newcond = ConditionFlag::ne; break;
			case ConditionFlag::ne: newcond = ConditionFlag::e ; break;
			case ConditionFlag::l : newcond = ConditionFlag::ge; break;
			case ConditionFlag::le: newcond = ConditionFlag::g ; break;
			case ConditionFlag::g : newcond = ConditionFlag::le; break;
			case ConditionFlag::ge: newcond = ConditionFlag::l ; break;
			case ConditionFlag::b : newcond = ConditionFlag::ae; break;
			case ConditionFlag::be: newcond = ConditionFlag::a ; break;
			case ConditionFlag::a : newcond = ConditionFlag::be; break;
			case ConditionFlag::ae: newcond = ConditionFlag::b ; break;
		}
		return {builder, reg, operand, newcond};
	}

	void compare() {
		llvm::Value *val = builder.CreateLoad(reg, "load");
		llvm::Value *other;
		switch(operand.index()){
			case 0: other = builder.CreateLoad( std::get<llvm::Value*>(operand) ); break;
			case 1:
				other = llvm::ConstantInt::get(((llvm::PointerType*)reg->getType())->getElementType(), std::get<int>(operand));
				break;
		}
		switch(cond){
			case ConditionFlag::e : cmp_result = builder.CreateICmpEQ (val, other); break;
			case ConditionFlag::ne: cmp_result = builder.CreateICmpNE (val, other); break;
			case ConditionFlag::l : cmp_result = builder.CreateICmpSLT(val, other); break;
			case ConditionFlag::le: cmp_result = builder.CreateICmpSLE(val, other); break;
			case ConditionFlag::g : cmp_result = builder.CreateICmpSGT(val, other); break;
			case ConditionFlag::ge: cmp_result = builder.CreateICmpSGE(val, other); break;
			case ConditionFlag::b : cmp_result = builder.CreateICmpULT(val, other); break;
			case ConditionFlag::be: cmp_result = builder.CreateICmpULE(val, other); break;
			case ConditionFlag::a : cmp_result = builder.CreateICmpUGT(val, other); break;
			case ConditionFlag::ae: cmp_result = builder.CreateICmpUGE(val, other); break;
		}
	}
	void jump(llvm::BasicBlock *bb_success, llvm::BasicBlock *bb_fail) const {
		builder.CreateCondBr(cmp_result, bb_success, bb_fail);
	}
};

} // namespace

#endif
