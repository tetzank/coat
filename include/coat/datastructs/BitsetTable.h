#ifndef BITSETTABLE_H_
#define BITSETTABLE_H_

#include "coat/Struct.h"

class BitsetTable final {

#define MEMBERS(x) \
	x(uint64_t, min) \
	x(uint64_t, max) \
	x(uint64_t*, data)

DECLARE_PRIVATE(MEMBERS)
#undef MEMBERS

public:
	BitsetTable(uint64_t min, uint64_t max) : min(min), max(max), data((uint64_t*)calloc((max-min+1)/64 +1, sizeof(uint64_t))) {}
	~BitsetTable(){
		free(data);
	}

	void insert(uint64_t key){
		key -= min;
		data[key / 64] |= 1ULL << (key % 64);
	}
	bool lookup(uint64_t key) const {
		// key could be out-of-bounds, larger domain on probe side
		if(key < min || key > max) return false;
		key -= min;
		return data[key / 64] & (1ULL << (key % 64));
	}
};


#if defined(ENABLE_ASMJIT) || defined(ENABLE_LLVMJIT)
namespace coat {

template<>
struct has_custom_base<BitsetTable> : std::true_type {};

template<class CC>
struct StructBase<Struct<CC,BitsetTable>> {
	using BT = BitsetTable;

	template<typename Fn>
	void check(Value<CC,uint64_t> &key, Fn &&then){
		auto &self = static_cast<Struct<CC,BT>&>(*this);
		//FIXME: accessed each time
		auto min = self.template get_value<BT::member_min>();
		if_then(self.cc, key >= min, [&]{
			auto max = self.template get_reference<BT::member_max>();
			if_then(self.cc, key <= max, [&]{
				auto nkey = key - min;
				//TODO: bt instruction for asmjit
				auto data = self.template get_value<BT::member_data>();
				auto val = data[nkey >> clog2(8*sizeof(uint64_t))];
				nkey &= (8*sizeof(uint64_t)) - 1;
				Value testbit(self.cc, uint64_t(1), "testbit");
				testbit <<= nkey;
				testbit &= val;
				if_then(self.cc, testbit > 0, then);
			});
		});
	}
};

}
#endif

#endif
