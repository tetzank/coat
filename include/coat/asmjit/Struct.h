#ifndef COAT_ASMJIT_STRUCT_H_
#define COAT_ASMJIT_STRUCT_H_

#include <asmjit/asmjit.h>

#include "../constexpr_helper.h"
#include "coat/Function.h"
#include "Ref.h"


namespace coat {


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

	Struct(F &cc, const char *name="") : cc(cc) {
		reg = cc.newIntPtr(name);
	}
#ifdef PROFILING_SOURCE
	Struct(F &cc, T *val, const char *name="", const char *file=__builtin_FILE(), int line=__builtin_LINE()) : Struct(cc, name) {
		*this = D<T*>{val, file, line};
	}
	Struct(F &cc, const T *val, const char *name="", const char *file=__builtin_FILE(), int line=__builtin_LINE()) : Struct(cc, name) {
		*this = D<T*>{const_cast<T*>(val), file, line};
	}
#else
	Struct(F &cc, T *val, const char *name="") : Struct(cc, name) {
		*this = val;
	}
	Struct(F &cc, const T *val, const char *name="") : Struct(cc, name) {
		*this = const_cast<T*>(val);
	}
#endif

	operator const ::asmjit::x86::Gp&() const { return reg; }
	operator       ::asmjit::x86::Gp&()       { return reg; }

	// load base pointer
	Struct &operator=(const D<T*> &other){
		cc.mov(reg, ::asmjit::imm(OP));
		DL;
		offset = 0;
		return *this;
	}

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
	wrapper_type<F,std::tuple_element_t<I, typename T::types>> get_value(
#ifdef PROFILING_SOURCE
		const char *file=__builtin_FILE(), int line=__builtin_LINE()
#endif
	) const {
		using type = std::tuple_element_t<I, typename T::types>;
		wrapper_type<F,type> ret(cc);
		if constexpr(std::is_array_v<type>){
			// array decay to pointer, just add offset to struct pointer
			//TODO: could just use struct pointer with fixed offset, no need for new register, similar to nested struct
			cc.lea(ret.reg, ::asmjit::x86::ptr(reg, offset_of_v<I,typename T::types> + offset));
#ifdef PROFILING_SOURCE
			((PerfCompiler&)cc).attachDebugLine(file, line);
#endif
		}else if constexpr(std::is_arithmetic_v<std::remove_pointer_t<type>>){
#if 0
			//FIXME: VRegMem not defined for pointer types currently
			ret = get_reference<I>();
#else
			switch(sizeof(type)){
				case 1: cc.mov(ret.reg, ::asmjit::x86:: byte_ptr(reg, offset_of_v<I,typename T::types> + offset)); break;
				case 2: cc.mov(ret.reg, ::asmjit::x86:: word_ptr(reg, offset_of_v<I,typename T::types> + offset)); break;
				case 4: cc.mov(ret.reg, ::asmjit::x86::dword_ptr(reg, offset_of_v<I,typename T::types> + offset)); break;
				case 8: cc.mov(ret.reg, ::asmjit::x86::qword_ptr(reg, offset_of_v<I,typename T::types> + offset)); break;
			}
#endif
#ifdef PROFILING_SOURCE
			((PerfCompiler&)cc).attachDebugLine(file, line);
#endif
		}else if constexpr(std::is_pointer_v<type>){
			// pointer to struct, load pointer
			cc.mov(ret.reg, ::asmjit::x86::qword_ptr(reg, offset_of_v<I,typename T::types> + offset));
#ifdef PROFILING_SOURCE
			((PerfCompiler&)cc).attachDebugLine(file, line);
#endif
		}else{
			// nested struct
			ret.reg = reg; // pass ptr register
			ret.offset = offset + offset_of_v<I,typename T::types>; // change offset
		}
		return ret;
	}
};


} // namespace


#endif
