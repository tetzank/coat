#ifndef LLVMJIT_REF_H_
#define LLVMJIT_REF_H_



template<class T>
struct Ref<::llvm::IRBuilder<>,T> {
	using F = ::llvm::IRBuilder<>;
	using inner_type = T;

	llvm::IRBuilder<> &builder;
	llvm::Value *mem;

	llvm::Value *load() const { return builder.CreateLoad(mem, "memload"); }
	//void store(llvm::Value *v) { builder.CreateStore(v, mem); }
	llvm::Type *type() const { return ((llvm::PointerType*)mem->getType())->getElementType(); }


	Ref(llvm::IRBuilder<> &builder, llvm::Value *mem) : builder(builder), mem(mem) {}

	Ref &operator=(const T &other){
		builder.CreateStore(other.load(), mem);
		return *this;
	}
	Ref &operator=(int value){
		builder.CreateStore(llvm::ConstantInt::get(type(), value), mem);
		return *this;
	}

	// operators creating temporary virtual registers
	T operator<<(int amount) const { T tmp(builder, "tmp"); tmp = *this; tmp <<= amount; return tmp; }
	T operator>>(int amount) const { T tmp(builder, "tmp"); tmp = *this; tmp >>= amount; return tmp; }
	T operator+ (int amount) const { T tmp(builder, "tmp"); tmp = *this; tmp  += amount; return tmp; }
	T operator- (int amount) const { T tmp(builder, "tmp"); tmp = *this; tmp  -= amount; return tmp; }
	T operator& (int amount) const { T tmp(builder, "tmp"); tmp = *this; tmp  &= amount; return tmp; }
	T operator| (int amount) const { T tmp(builder, "tmp"); tmp = *this; tmp  |= amount; return tmp; }
	T operator^ (int amount) const { T tmp(builder, "tmp"); tmp = *this; tmp  ^= amount; return tmp; }

	//T operator*(const T &other) const { T tmp(builder, "tmp"); tmp = *this; tmp *= other; return tmp; }
	//T operator/(const T &other) const { T tmp(builder, "tmp"); tmp = *this; tmp /= other; return tmp; }
	//T operator%(const T &other) const { T tmp(builder, "tmp"); tmp = *this; tmp %= other; return tmp; }

	// comparisons
	// swap sides of operands and comparison, not needed for assembly, but avoids code duplication in wrapper
	Condition<F> operator==(const T &other) const { return other==*this; }
	Condition<F> operator!=(const T &other) const { return other!=*this; }
	Condition<F> operator< (const T &other) const { return other>=*this; }
	Condition<F> operator<=(const T &other) const { return other> *this; }
	Condition<F> operator> (const T &other) const { return other<=*this; }
	Condition<F> operator>=(const T &other) const { return other< *this; }
	//TODO: possible without temporary: cmp m32 imm32, complicates Condition
	Condition<F> operator==(int constant) const { T tmp(builder, "tmp"); tmp = *this; return tmp==constant; }
	Condition<F> operator!=(int constant) const { T tmp(builder, "tmp"); tmp = *this; return tmp!=constant; }
	Condition<F> operator< (int constant) const { T tmp(builder, "tmp"); tmp = *this; return tmp< constant; }
	Condition<F> operator<=(int constant) const { T tmp(builder, "tmp"); tmp = *this; return tmp<=constant; }
	Condition<F> operator> (int constant) const { T tmp(builder, "tmp"); tmp = *this; return tmp> constant; }
	Condition<F> operator>=(int constant) const { T tmp(builder, "tmp"); tmp = *this; return tmp>=constant; }
	// not possible in instruction, requires temporary
	Condition<F> operator==(const Ref &other) const { T tmp(builder, "tmp"); tmp = *this; return tmp==other; }
	Condition<F> operator!=(const Ref &other) const { T tmp(builder, "tmp"); tmp = *this; return tmp!=other; }
	Condition<F> operator< (const Ref &other) const { T tmp(builder, "tmp"); tmp = *this; return tmp< other; }
	Condition<F> operator<=(const Ref &other) const { T tmp(builder, "tmp"); tmp = *this; return tmp<=other; }
	Condition<F> operator> (const Ref &other) const { T tmp(builder, "tmp"); tmp = *this; return tmp> other; }
	Condition<F> operator>=(const Ref &other) const { T tmp(builder, "tmp"); tmp = *this; return tmp>=other; }
};


#endif
