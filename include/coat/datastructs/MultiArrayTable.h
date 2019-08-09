#ifndef MULTIARRAYTABLE_H_
#define MULTIARRAYTABLE_H_

#include "coat/Struct.h"


template<typename T>
class MultiArrayTable final {

#define MEMBERS(x) \
	x(T, min) \
	x(T, max) \
	x(T*, offsets) \
	x(T*, rows)

DECLARE_PRIVATE(MEMBERS)
#undef MEMBERS

public:
	MultiArrayTable(T min, T max, T *col, size_t size) : min(min), max(max) {
		// frequency of values
		size_t offset_size = max - min + 2;
		offsets = (T*)calloc(offset_size, sizeof(T));
		for(size_t i=0; i<size; ++i){
			++offsets[col[i] - min];
		}
		// create offsets into rows array, prefix sum
		// set to end position, so that insertion starts at end, no linear search for open slot
		T prefixSum=0;
		for(T *pos=offsets, *end=offsets+offset_size; pos!=end; ++pos){
			prefixSum += *pos;
			*pos = prefixSum; // end position
		}
		// fill rows array
		rows = (T*)malloc(size * sizeof(T));
		for(uint64_t i=0; i<size; ++i){
			//TODO: bottleneck during construction, random access, writing just one entry
			rows[--offsets[col[i] - min]] = i;
		}
	}
	~MultiArrayTable(){
		free(offsets);
		free(rows);
	}

	std::pair<T*,T*> lookupIterators(size_t key) const {
		if(key >= min && key <= max){
			return {rows + offsets[key - min], rows + offsets[key-min + 1]};
		}else{
			return {nullptr, nullptr};
		}
	}
};


#if defined(ENABLE_ASMJIT) || defined(ENABLE_LLVMJIT)
namespace coat {

template<typename T>
struct has_custom_base<MultiArrayTable<T>> : std::true_type {};

template<class CC, typename T>
struct StructBase<Struct<CC,MultiArrayTable<T>>> {
	using AT = MultiArrayTable<T>;

	template<typename Fn>
	void iterate(Value<CC,size_t> &key, Fn &&then) {
		auto &self = static_cast<Struct<CC,AT>&>(*this);
		//FIXME: accessed each time
		auto min = self.template get_value<AT::member_min>();
		if_then(self.cc, key >= min, [&]{
			auto max = self.template get_reference<AT::member_max>();
			if_then(self.cc, key <= max, [&]{
				auto nkey = key - min;
				auto offsets = self.template get_value<AT::member_offsets>();
				auto rows = self.template get_value<AT::member_rows>();
				Value<CC,T> beg_offsets(self.cc);
				beg_offsets = offsets[nkey];
				Value<CC,T> end_offsets(self.cc);
				end_offsets = offsets[nkey+1];
				auto beg = rows + beg_offsets;
				auto end = rows + end_offsets;
				for_each(self.cc, beg, end, then);
			});
		});

	}
};
}
#endif

#endif
