#ifndef COAT_STRUCT_H_
#define COAT_STRUCT_H_

#include <tuple>

namespace coat {

#define STRUCT_MEMBER(ty, id) ty id;
#define ENUM_MEMBER(ty, id) member_##id,
#define STRING_MEMBER(ty, id) #id,
#define TYPE_MEMBER(ty, id) ty,

// declares (public) members, enum and tuple containing types
#define DECLARE(members)                       \
	members(STRUCT_MEMBER)                     \
	enum member_ids : int {                    \
		members(ENUM_MEMBER)                   \
	};                                         \
	static constexpr std::array member_names { \
		members(STRING_MEMBER)                 \
	};                                         \
	using types = std::tuple<                  \
		members(TYPE_MEMBER)                   \
	void>;

// declares private members and public enum and types
#define DECLARE_PRIVATE(members)               \
private:                                       \
	members(STRUCT_MEMBER)                     \
public:                                        \
	enum member_ids : int {                    \
		members(ENUM_MEMBER)                   \
	};                                         \
	static constexpr std::array member_names { \
		members(STRING_MEMBER)                 \
	};                                         \
	using types = std::tuple<                  \
		members(TYPE_MEMBER)                   \
	void>;


template<typename T>
struct has_custom_base : std::false_type {};

template<typename T>
struct StructBase;

struct StructBaseEmpty {};


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
