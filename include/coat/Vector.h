#ifndef COAT_VECTOR_H_
#define COAT_VECTOR_H_

namespace coat {

template<class CC, typename T, unsigned width>
struct Vector;

template<class CC, unsigned width>
struct VectorMask;

} // namespace

#ifdef ENABLE_ASMJIT
#  include "asmjit/Vector.h"
#endif
#ifdef ENABLE_LLVMJIT
#  include "llvmjit/Vector.h"
#endif

#endif
