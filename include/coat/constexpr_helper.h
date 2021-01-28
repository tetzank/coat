#ifndef COAT_CONSTEXPR_HELPER_H_
#define COAT_CONSTEXPR_HELPER_H_

#include <type_traits>
#include <utility>
#include <cstddef>


namespace coat {

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


// reimplementation of offsetof() in constexpr

constexpr size_t roundup(size_t num, size_t multiple){
	const size_t mod = num % multiple;
	return (mod==0)? num: num + multiple - mod;
}

template<size_t I, typename Tuple>
struct offset_of {
	static constexpr size_t value = roundup(
		offset_of<I-1, Tuple>::value + sizeof(std::tuple_element_t<I-1, Tuple>),
		alignof(std::tuple_element_t<I, Tuple>)
	);
};

template<typename Tuple>
struct offset_of<0, Tuple> {
	static constexpr size_t value = 0;
};

template<size_t I, typename Tuple>
constexpr size_t offset_of_v = offset_of<I, Tuple>::value;

// end of reimplementation of offsetof()

} // namespace

#endif
