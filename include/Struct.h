#ifndef COAT_STRUCT_H_
#define COAT_STRUCT_H_


namespace coat {

#define STRUCT_MEMBER(ty, id) ty id;
#define ENUM_MEMBER(ty, id) member_##id,
#define TYPE_MEMBER(ty, id) ty,

#define DECLARE(members)      \
	members(STRUCT_MEMBER)    \
	enum member_ids : int {   \
		members(ENUM_MEMBER)  \
	};                        \
	using types = std::tuple< \
		members(TYPE_MEMBER)  \
	void>;


template<class CC, typename T>
struct Struct;

} // namespace

#ifdef ENABLE_ASMJIT
#  include "asmjit/Struct.h"
#endif

#ifdef ENABLE_LLVMJIT
#  include "llvmjit/Struct.h"
#endif


#endif
