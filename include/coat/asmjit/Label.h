#ifndef COAT_ASMJIT_LABEL_H_
#define COAT_ASMJIT_LABEL_H_

#include <asmjit/asmjit.h>


namespace coat {

template<>
struct Label<::asmjit::x86::Compiler> final {
	using F = ::asmjit::x86::Compiler;

	::asmjit::x86::Compiler &cc;
	::asmjit::Label label;

	Label(::asmjit::x86::Compiler &cc) : cc(cc), label(cc.newLabel()) {}

	void bind() {
		cc.bind(label);
	}

	operator const ::asmjit::Label&() const { return label; }
	operator       ::asmjit::Label&()       { return label; }
};

// deduction guides
template<typename FnPtr> Label(Function<runtimeasmjit,FnPtr>&) -> Label<::asmjit::x86::Compiler>;

} // namespace

#endif
