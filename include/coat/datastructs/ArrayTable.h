#ifndef ARRAYTABLE_H_
#define ARRAYTABLE_H_

#include <cstring> // memset

#include "coat/Struct.h"


template<typename T>
class ArrayTable final {

#define MEMBERS(x) \
	x(T, min) \
	x(T, max) \
	x(T*, arr)

DECLARE_PRIVATE(MEMBERS)
#undef MEMBERS


public:
	ArrayTable(T min, T max) : min(min), max(max), arr(new T[max - min + 1]) {
		memset(arr, 0xff, (max-min+1) * sizeof(T));
	}
	ArrayTable(T min, T max, T *col, size_t size) : ArrayTable(min, max) {
		for(size_t i=0; i<size; ++i){
			arr[col[i] - min] = i;
		}
	}
	~ArrayTable(){
		delete[] arr;
	}

	void insert(size_t key, T data){
		arr[key - min] = data;
	}

	T lookup(size_t key) const {
		// bounds checking
		if(key >= min && key <= max){
			return arr[key - min];
		}else{
			return end();
		}
	}
	constexpr T end() const {
		return std::numeric_limits<T>::max();
	}
};


#if defined(ENABLE_ASMJIT) || defined(ENABLE_LLVMJIT)
namespace coat {

template<typename T>
struct has_custom_base<ArrayTable<T>> : std::true_type {};

template<class CC, typename T>
struct StructBase<Struct<CC,ArrayTable<T>>> {
	using AT = ArrayTable<T>;

	template<typename Fn>
	void lookup(Value<CC,size_t> &key, Fn &&then){
		auto &self = static_cast<Struct<CC,AT>&>(*this);
		//FIXME: accessed each time
		auto arr = self.template get_value<AT::member_arr>();
		// bounds checking
		//TODO: missing combinatorial operation && and || between conditions, with short circuit semantics
		auto min = self.template get_value<AT::member_min>();
		if_then(self.cc, key >= min, [&]{
			auto max = self.template get_reference<AT::member_max>();
			if_then(self.cc, key <= max, [&]{
				auto val = arr[key - min];
				then(val);
			});
		});
	}
};

}
#endif

#endif
