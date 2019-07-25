#ifndef REF_H_
#define REF_H_


template<class CC, typename T>
struct Ref;


#ifdef ENABLE_ASMJIT
#  include "asmjit/Ref.h"
#endif

#ifdef ENABLE_LLVMJIT
#  include "llvmjit/Ref.h"
#endif


#endif
