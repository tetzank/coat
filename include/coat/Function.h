#ifndef COAT_FUNCTION_H_
#define COAT_FUNCTION_H_

#include <type_traits>


namespace coat {

//FIXME: forwards
template<typename F, typename T> struct Value;
template<typename F, typename T> struct Ptr;
template<typename F, typename T> struct Struct;


template<typename F, typename T>
using reg_type = std::conditional_t<std::is_pointer_v<T>,
						Ptr<F,Value<F,std::remove_cv_t<std::remove_pointer_t<T>>>>,
						Value<F,std::remove_cv_t<T>>
				>;

// decay - converts array types to pointer types
template<typename F, typename T>
using wrapper_type = std::conditional_t<std::is_arithmetic_v<std::remove_pointer_t<std::decay_t<T>>>,
						reg_type<F,std::decay_t<T>>,
						Struct<F,std::remove_cv_t<std::remove_pointer_t<T>>>
					>;


template<typename T, typename F>
struct Function;

template<typename T, typename F>
struct InternalFunction;

} // namespace


#ifdef ENABLE_ASMJIT
#  include "asmjit/Function.h"
#endif

#ifdef ENABLE_LLVMJIT
#  include "llvmjit/Function.h"
#endif


#include "Condition.h"
#include "Ref.h"
#include "Value.h"
#include "Ptr.h"
#include "Struct.h"

#endif
