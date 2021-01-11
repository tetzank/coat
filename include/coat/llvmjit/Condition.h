#ifndef COAT_LLVMJIT_CONDITION_H_
#define COAT_LLVMJIT_CONDITION_H_

#include <variant>


namespace coat {

template<>
struct Condition<LLVMBuilders> {
	LLVMBuilders &cc;
	llvm::Value *cmp_result;
	llvm::Value *reg;
	using operand_t = std::variant<llvm::Value*,int>;
	operand_t operand;
	ConditionFlag cond;

	Condition(LLVMBuilders &cc, llvm::Value *reg, operand_t operand, ConditionFlag cond)
		: cc(cc), reg(reg), operand(operand), cond(cond) {}

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
		return {cc, reg, operand, newcond};
	}

	void compare() {
		llvm::Value *val = cc.ir.CreateLoad(reg, "load");
		llvm::Value *other;
		switch(operand.index()){
			case 0: other = cc.ir.CreateLoad( std::get<llvm::Value*>(operand) ); break;
			case 1:
				other = llvm::ConstantInt::get(((llvm::PointerType*)reg->getType())->getElementType(), std::get<int>(operand));
				break;
			default:
				__builtin_trap(); // should not be reached
		}
		switch(cond){
			case ConditionFlag::e : cmp_result = cc.ir.CreateICmpEQ (val, other); break;
			case ConditionFlag::ne: cmp_result = cc.ir.CreateICmpNE (val, other); break;
			case ConditionFlag::l : cmp_result = cc.ir.CreateICmpSLT(val, other); break;
			case ConditionFlag::le: cmp_result = cc.ir.CreateICmpSLE(val, other); break;
			case ConditionFlag::g : cmp_result = cc.ir.CreateICmpSGT(val, other); break;
			case ConditionFlag::ge: cmp_result = cc.ir.CreateICmpSGE(val, other); break;
			case ConditionFlag::b : cmp_result = cc.ir.CreateICmpULT(val, other); break;
			case ConditionFlag::be: cmp_result = cc.ir.CreateICmpULE(val, other); break;
			case ConditionFlag::a : cmp_result = cc.ir.CreateICmpUGT(val, other); break;
			case ConditionFlag::ae: cmp_result = cc.ir.CreateICmpUGE(val, other); break;
		}
	}
	void jump(llvm::BasicBlock *bb_success, llvm::BasicBlock *bb_fail) const {
		cc.ir.CreateCondBr(cmp_result, bb_success, bb_fail);
	}
};

} // namespace

#endif
