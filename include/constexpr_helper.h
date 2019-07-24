#ifndef CONSTEXPR_HELPER_H_
#define CONSTEXPR_HELPER_H_

#include <type_traits>


inline constexpr bool is_power_of_two(unsigned value){
	return (value & (value-1)) == 0;
}

// helper function, not defined for long as it is only used to convert literals
inline constexpr int clog2(int x){
	static_assert(sizeof(int)==4, "int not 32 bit wide");
	return (8*sizeof(x) - 1) - __builtin_clz(x);
}

// for use in if-constexpr() and static_assert()
template<typename T> constexpr std::false_type should_not_be_reached{};


#endif
