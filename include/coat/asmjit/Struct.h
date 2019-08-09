#ifndef COAT_ASMJIT_STRUCT_H_
#define COAT_ASMJIT_STRUCT_H_

#include <asmjit/asmjit.h>

#include "coat/Function.h"
#include "Ref.h"


namespace coat {

// reimplementation of offsetof() in constexpr

constexpr size_t roundup(size_t num, size_t multiple){
	const size_t mod = num % multiple;
	return (mod==0)? num: num + multiple - mod;
}

template<size_t I, typename Tuple>
struct offset_of {
	static constexpr size_t value = roundup(
		offset_of<I-1, Tuple>::value + sizeof(std::tuple_element_t<I-1, Tuple>),
		alignof(std::tuple_element_t<I, Tuple>)
	);
};

template<typename Tuple>
struct offset_of<0, Tuple> {
	static constexpr size_t value = 0;
};

template<size_t I, typename Tuple>
constexpr size_t offset_of_v = offset_of<I, Tuple>::value;

// end of reimplementation of offsetof()


template<typename T>
struct Struct<::asmjit::x86::Compiler,T>
	: public std::conditional_t<has_custom_base<T>::value,
								StructBase<Struct<::asmjit::x86::Compiler,T>>,
								StructBaseEmpty
			>
{
	using F = ::asmjit::x86::Compiler;
	using struct_type = T;

	static_assert(std::is_standard_layout_v<T>, "wrapped class needs to have standard layout");

	::asmjit::x86::Compiler &cc; //FIXME: pointer stored in every value type
	::asmjit::x86::Gp reg;
	size_t offset=0;

	Struct(::asmjit::x86::Compiler &cc, const char *name="") : cc(cc) {
		reg = cc.newIntPtr(name);
	}

	operator const ::asmjit::x86::Gp&() const { return reg; }
	operator       ::asmjit::x86::Gp&()       { return reg; }

	// load base pointer
	Struct &operator=(T *ptr){ cc.mov(reg, ::asmjit::imm(ptr)); offset = 0; return *this; }

	// pre-increment
	Struct &operator++(){ cc.add(reg, sizeof(T)); return *this; }

	Struct operator+ (int amount) const {
		Struct res(cc);
		cc.lea(res, ::asmjit::x86::ptr(reg, amount*sizeof(T)));
		return res;
	}

	template<int I>
	Ref<F,reg_type<F,std::tuple_element_t<I, typename T::types>>> get_reference(){
		switch(sizeof(std::tuple_element_t<I, typename T::types>)){
			case 1: return {cc, ::asmjit::x86:: byte_ptr(reg, offset_of_v<I, typename T::types> + offset)};
			case 2: return {cc, ::asmjit::x86:: word_ptr(reg, offset_of_v<I, typename T::types> + offset)};
			case 4: return {cc, ::asmjit::x86::dword_ptr(reg, offset_of_v<I, typename T::types> + offset)};
			case 8: return {cc, ::asmjit::x86::qword_ptr(reg, offset_of_v<I, typename T::types> + offset)};
		}
	}

	template<int I>
	wrapper_type<F,std::tuple_element_t<I, typename T::types>> get_value() const {
		wrapper_type<F,std::tuple_element_t<I, typename T::types>> ret(cc);
		if constexpr(std::is_arithmetic_v<std::remove_pointer_t<std::tuple_element_t<I, typename T::types>>>){
#if 0
			//FIXME: VRegMem not defined for pointer types currently
			ret = get_reference<I>();
#else
			switch(sizeof(std::tuple_element_t<I, typename T::types>)){
				case 1: cc.mov(ret.reg, ::asmjit::x86:: byte_ptr(reg, offset_of_v<I,typename T::types> + offset)); break;
				case 2: cc.mov(ret.reg, ::asmjit::x86:: word_ptr(reg, offset_of_v<I,typename T::types> + offset)); break;
				case 4: cc.mov(ret.reg, ::asmjit::x86::dword_ptr(reg, offset_of_v<I,typename T::types> + offset)); break;
				case 8: cc.mov(ret.reg, ::asmjit::x86::qword_ptr(reg, offset_of_v<I,typename T::types> + offset)); break;
			}
#endif
		}else{
			if constexpr(std::is_pointer_v<std::tuple_element_t<I, typename T::types>>){
				// pointer to struct, load pointer
				cc.mov(ret.reg, ::asmjit::x86::qword_ptr(reg, offset_of_v<I,typename T::types> + offset));
			}else{
				// nested struct
				ret.reg = reg; // pass ptr register
				ret.offset = offset + offset_of_v<I,typename T::types>; // change offset
			}
		}
		return ret;
	}
};


} // namespace


#endif
