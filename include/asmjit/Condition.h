#ifndef ASMJIT_CONDITION_H_
#define ASMJIT_CONDITION_H_

#include <variant>

#include <asmjit/asmjit.h>



//TODO: combinations of conditions not possible, e.g. "i<j && r<s"
//      really needs expressiont tree in the end

// holds operands and comparison type
// cannot emit instructions directly as comparison emits multiple instructions at different locations
// while-loop: if(cond) do{ ... }while(cond);
template<>
struct Condition<::asmjit::x86::Compiler> {
	::asmjit::x86::Compiler &cc; //FIXME: pointer stored in every value type
	// take by value as it might be a temporary which we have to store otherwise it's gone
	::asmjit::x86::Gp reg;
	//const ::asmjit::Operand &operand;
	using operand_t = std::variant<::asmjit::x86::Gp,int,::asmjit::x86::Mem>;
	operand_t operand;
	ConditionFlag cond;

	Condition(::asmjit::x86::Compiler &cc, ::asmjit::x86::Gp reg, operand_t operand, ConditionFlag cond)
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

			default:
				__builtin_trap(); //FIXME: crash
		}
		return {cc, reg, operand, newcond};
	}

	void compare() const {
		switch(operand.index()){
			case 0: cc.cmp(reg, std::get<::asmjit::x86::Gp>(operand)); break;
			case 1: cc.cmp(reg, ::asmjit::imm(std::get<int>(operand))); break;
			case 2: cc.cmp(reg, std::get<::asmjit::x86::Mem>(operand)); break;
		}
	}
	void setbyte(::asmjit::x86::Gp &dest) const {
		switch(cond){
			case ConditionFlag::e : cc.sete (dest); break;
			case ConditionFlag::ne: cc.setne(dest); break;
			case ConditionFlag::l : cc.setl (dest); break;
			case ConditionFlag::le: cc.setle(dest); break;
			case ConditionFlag::g : cc.setg (dest); break;
			case ConditionFlag::ge: cc.setge(dest); break;
			case ConditionFlag::b : cc.setb (dest); break;
			case ConditionFlag::be: cc.setbe(dest); break;
			case ConditionFlag::a : cc.seta (dest); break;
			case ConditionFlag::ae: cc.setae(dest); break;
		}
	}
	void jump(::asmjit::Label label) const {
		switch(cond){
			case ConditionFlag::e : cc.je (label); break;
			case ConditionFlag::ne: cc.jne(label); break;
			case ConditionFlag::l : cc.jl (label); break;
			case ConditionFlag::le: cc.jle(label); break;
			case ConditionFlag::g : cc.jg (label); break;
			case ConditionFlag::ge: cc.jge(label); break;
			case ConditionFlag::b : cc.jb (label); break;
			case ConditionFlag::be: cc.jbe(label); break;
			case ConditionFlag::a : cc.ja (label); break;
			case ConditionFlag::ae: cc.jae(label); break;
		}
	}
};


#endif
