#ifndef COAT_STRUCT_H_
#define COAT_STRUCT_H_

#include <tuple>

namespace coat {

#define COAT_NAME(str) public: static constexpr const char *name = str

#define COAT_STRUCT_MEMBER(ty, id) ty id;
#define COAT_ENUM_MEMBER(ty, id) member_##id,
#define COAT_STRING_MEMBER(ty, id) #id,
#define COAT_TYPE_MEMBER(ty, id) ty,

// declares (public) members, enum and tuple containing types
#define COAT_DECLARE(members)                  \
	members(COAT_STRUCT_MEMBER)                \
	enum member_ids : int {                    \
		members(COAT_ENUM_MEMBER)              \
	};                                         \
	static constexpr std::array member_names { \
		members(COAT_STRING_MEMBER)            \
	};                                         \
	using types = std::tuple<                  \
		members(COAT_TYPE_MEMBER)              \
	void>;

// declares private members and public enum and types
#define COAT_DECLARE_PRIVATE(members)          \
private:                                       \
	members(COAT_STRUCT_MEMBER)                \
public:                                        \
	enum member_ids : int {                    \
		members(COAT_ENUM_MEMBER)              \
	};                                         \
	static constexpr std::array member_names { \
		members(COAT_STRING_MEMBER)            \
	};                                         \
	using types = std::tuple<                  \
		members(COAT_TYPE_MEMBER)              \
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
