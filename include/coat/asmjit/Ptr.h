#ifndef COAT_ASMJIT_PTR_H_
#define COAT_ASMJIT_PTR_H_

#include "ValueBase.h"
//#include "Ref.h"

#include "../constexpr_helper.h"


namespace coat {

template<class T>
struct Ptr<::asmjit::x86::Compiler,T>{
	using F = ::asmjit::x86::Compiler;
	using value_type = typename T::value_type;
	using value_base_type = ValueBase<F>;
	using mem_type = Ref<F,T>;

	static_assert(std::is_base_of_v<value_base_type,T>, "pointer type only of value wrappers");

	::asmjit::x86::Compiler &cc; //FIXME: pointer stored in every value type
	::asmjit::x86::Gp reg;

	Ptr(F &cc, const char *name="") : cc(cc) {
		reg = cc.newIntPtr(name);
	}

#ifdef PROFILING_SOURCE
	Ptr(F &cc, value_type *val, const char *name="", const char *file=__builtin_FILE(), int line=__builtin_LINE()) : Ptr(cc, name) {
		*this = D<value_type*>{val, file, line};
	}
	Ptr(F &cc, const value_type *val, const char *name="", const char *file=__builtin_FILE(), int line=__builtin_LINE()) : Ptr(cc, name) {
		*this = D<value_type*>{const_cast<value_type*>(val), file, line};
	}
#else
	Ptr(F &cc, value_type *val, const char *name="") : Ptr(cc, name) {
		*this = val;
	}
	Ptr(F &cc, const value_type *val, const char *name="") : Ptr(cc, name) {
		*this = const_cast<value_type*>(val);
	}
#endif

	// real copy requires new register and copy of content
	Ptr(const Ptr &other) : Ptr(other.cc) {
		*this = other;
	}
	// move, just take the register
	Ptr(const Ptr &&other) : cc(other.cc), reg(other.reg) {}

	// assignment
	Ptr &operator=(const D<value_type*> &other){
		cc.mov(reg, ::asmjit::imm(OP));
		DL;
		return *this;
	}
	Ptr &operator=(const Ptr &other){
		cc.mov(reg, other.reg);
		return *this;
	}

	void setName(const char * /*name*/){
		//TODO
	}

	operator const ::asmjit::x86::Gp&() const { return reg; }
	operator       ::asmjit::x86::Gp&()       { return reg; }

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

	Ptr operator+(const D<value_base_type> &other) const {
		Ptr res(cc);
		cc.lea(res, ::asmjit::x86::ptr(reg, OP, clog2(sizeof(value_type))));
		DL;
		return res;
	}
	Ptr operator+(size_t value) const {
		Ptr res(cc);
		cc.lea(res, ::asmjit::x86::ptr(reg, value*sizeof(value_type)));
		return res;
	}

	Ptr &operator+=(const value_base_type &value){
		cc.lea(reg, ::asmjit::x86::ptr(reg, value.reg, clog2(sizeof(value_type))));
		return *this;
	}
	Ptr &operator+=(const D<int> &other){ cc.add(reg, OP*sizeof(value_type)); DL; return *this; }
	Ptr &operator-=(int amount){ cc.sub(reg, amount*sizeof(value_type)); return *this; }

	// like "+=" without pointer arithmetic
	Ptr &addByteOffset(const value_base_type &value){ //TODO: any integer value should be possible as operand
		cc.lea(reg, ::asmjit::x86::ptr(reg, value.reg));
		return *this;
	}

	// operators creating temporary virtual registers
	Value<F,size_t> operator- (const Ptr &other) const {
		Value<F,size_t> ret(cc, "ret");
		cc.mov(ret, reg);
		cc.sub(ret, other.reg);
		cc.sar(ret, clog2(sizeof(value_type))); // compilers also do arithmetic shift...
		return ret;
	}

	// pre-increment, post-increment not provided as it creates temporary
	Ptr &operator++(){ cc.add(reg, sizeof(value_type)); return *this; }
	// pre-decrement
	Ptr &operator--(){ cc.sub(reg, sizeof(value_type)); return *this; }

	// comparisons
	Condition<F> operator==(const Ptr &other) const { return {cc, reg, other.reg, ConditionFlag::e};  }
	Condition<F> operator!=(const Ptr &other) const { return {cc, reg, other.reg, ConditionFlag::ne}; }
};


template<typename dest_type, typename src_type>
Ptr<::asmjit::x86::Compiler,Value<::asmjit::x86::Compiler,std::remove_pointer_t<dest_type>>>
cast(const Ptr<::asmjit::x86::Compiler,Value<::asmjit::x86::Compiler,src_type>> &src){
	static_assert(std::is_pointer_v<dest_type>, "a pointer type can only be casted to another pointer type");

	//TODO: find a way to do it without copies but no surprises for user
	// create new pointer with new register
	Ptr<::asmjit::x86::Compiler,Value<::asmjit::x86::Compiler,std::remove_pointer_t<dest_type>>> res(src.cc);
	// copy pointer address between registers
	src.cc.mov(res.reg, src.reg);
	// return new pointer
	return res;
}

} // namespace

#endif
