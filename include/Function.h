#ifndef FUNCTION_H_
#define FUNCTION_H_

#include <type_traits>


//FIXME: forwards
template<typename F, typename T> struct Value;
template<typename F, typename T> struct Ptr;
template<typename F, typename T> struct Struct;


template<typename F, typename T>
using reg_type = std::conditional_t<std::is_pointer_v<T>,
						Ptr<F,Value<F,std::remove_cv_t<std::remove_pointer_t<T>>>>,
						Value<F,std::remove_cv_t<T>>
				>;

template<typename F, typename T>
using wrapper_type = std::conditional_t<std::is_arithmetic_v<std::remove_pointer_t<T>>,
						reg_type<F,T>,
						Struct<F,std::remove_cv_t<std::remove_pointer_t<T>>>
					>;


template<typename T, typename F>
struct Function;

#ifdef ENABLE_ASMJIT
#  include "asmjit/Function.h"
#endif


#include "Value.h"
#include "Ptr.h"

#endif
