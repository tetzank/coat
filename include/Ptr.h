#ifndef COAT_PTR_H_
#define COAT_PTR_H_

namespace coat {

template<class CC, typename T>
struct Ptr;

} // namespace

#ifdef ENABLE_ASMJIT
#  include "asmjit/Ptr.h"
#endif

#ifdef ENABLE_LLVMJIT
#  include "llvmjit/Ptr.h"
#endif


#endif
