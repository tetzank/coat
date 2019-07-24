#ifndef ASMJIT_PTR_H_
#define ASMJIT_PTR_H_

#include "ValueBase.h"
//#include "Ref.h"

#include "../constexpr_helper.h"



template<class T>
struct Ptr<::asmjit::x86::Compiler,T>{
	using value_type = typename T::value_type;
	using value_base_type = ValueBase<::asmjit::x86::Compiler>;
	//using mem_type = Ref<T>;

	static_assert(std::is_base_of_v<value_base_type,T>, "pointer type only of value wrappers");

	::asmjit::x86::Compiler &cc; //FIXME: pointer stored in every value type
	::asmjit::x86::Gp reg;

	Ptr(::asmjit::x86::Compiler &cc, const char *name="") : cc(cc) {
		reg = cc.newIntPtr(name);
	}
	Ptr(const Ptr &other) : cc(other.cc), reg(other.reg) {}

	Ptr &operator=(value_type *value){
		cc.mov(reg, ::asmjit::imm(value));
		return *this;
	}

	operator const ::asmjit::x86::Gp&() const { return reg; }
	operator       ::asmjit::x86::Gp&()       { return reg; }

#if 0
	// dereference
	mem_type operator*(){
		switch(sizeof(value_type)){
			case 1: return {cc, ::asmjit::x86::byte_ptr (reg)};
			case 2: return {cc, ::asmjit::x86::word_ptr (reg)};
			case 4: return {cc, ::asmjit::x86::dword_ptr(reg)};
			case 8: return {cc, ::asmjit::x86::qword_ptr(reg)};
		}
	}
	// indexing with variable
	mem_type operator[](const value_base_type &idx){
		switch(sizeof(value_type)){
			case 1: return {cc, ::asmjit::x86::byte_ptr (reg, idx.reg, clog2(sizeof(value_type)))};
			case 2: return {cc, ::asmjit::x86::word_ptr (reg, idx.reg, clog2(sizeof(value_type)))};
			case 4: return {cc, ::asmjit::x86::dword_ptr(reg, idx.reg, clog2(sizeof(value_type)))};
			case 8: return {cc, ::asmjit::x86::qword_ptr(reg, idx.reg, clog2(sizeof(value_type)))};
		}
	}
	// indexing with constant -> use offset
	mem_type operator[](int idx){
		switch(sizeof(value_type)){
			case 1: return {cc, ::asmjit::x86::byte_ptr (reg, idx*sizeof(value_type))};
			case 2: return {cc, ::asmjit::x86::word_ptr (reg, idx*sizeof(value_type))};
			case 4: return {cc, ::asmjit::x86::dword_ptr(reg, idx*sizeof(value_type))};
			case 8: return {cc, ::asmjit::x86::qword_ptr(reg, idx*sizeof(value_type))};
		}
	}
	// get memory operand with displacement
	mem_type byteOffset(long offset){
		switch(sizeof(value_type)){
			case 1: return {cc, ::asmjit::x86::byte_ptr (reg, offset)};
			case 2: return {cc, ::asmjit::x86::word_ptr (reg, offset)};
			case 4: return {cc, ::asmjit::x86::dword_ptr(reg, offset)};
			case 8: return {cc, ::asmjit::x86::qword_ptr(reg, offset)};
		}
	}
#endif

	Ptr operator+(const value_base_type &value) const {
		Ptr res(cc);
		cc.lea(res, ::asmjit::x86::ptr(reg, value.reg, clog2(sizeof(value_type))));
		return res;
	}
	Ptr operator+(size_t value) const {
		Ptr res(cc);
		cc.lea(res, ::asmjit::x86::ptr(reg, value*sizeof(value_type)));
		return res;
	}

	Ptr &operator+=(int amount){ cc.add(reg, amount*sizeof(value_type)); return *this; }
	Ptr &operator-=(int amount){ cc.sub(reg, amount*sizeof(value_type)); return *this; }

	// pre-increment, post-increment not provided as it creates temporary
	Ptr &operator++(){ cc.add(reg, sizeof(value_type)); return *this; }
	// pre-decrement
	Ptr &operator--(){ cc.sub(reg, sizeof(value_type)); return *this; }

	// comparisons
	//Condition operator==(const Ptr &other) const { return {cc, reg, other.reg, Condition::ConditionFlag::e};  }
	//Condition operator!=(const Ptr &other) const { return {cc, reg, other.reg, Condition::ConditionFlag::ne}; }
};


#endif
