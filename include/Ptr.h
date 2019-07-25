#ifndef PTR_H_
#define PTR_H_


template<class CC, typename T>
struct Ptr;


#ifdef ENABLE_ASMJIT
#  include "asmjit/Ptr.h"
#endif

#ifdef ENABLE_LLVMJIT
#  include "llvmjit/Ptr.h"
#endif


#endif
