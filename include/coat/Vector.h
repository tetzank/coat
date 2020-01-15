#ifndef COAT_VECTOR_H_
#define COAT_VECTOR_H_

namespace coat {

template<class CC, typename T, unsigned width>
struct Vector;

} // namespace

#ifdef ENABLE_ASMJIT
#  include "asmjit/Vector.h"
#endif

#endif
