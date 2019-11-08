#ifndef COAT_ASMJIT_DEBUGOPERAND_H_
#define COAT_ASMJIT_DEBUGOPERAND_H_


namespace coat {

// it just carries the source file and line number, additionally to the operand
template<typename T>
struct DebugOperand {
	const T &operand;
	const char *file;
	int line;

	DebugOperand(const T &operand, const char *file=__builtin_FILE(), int line=__builtin_LINE())
		: operand(operand), file(file), line(line) {}
};

//HACK
#ifdef NDEBUG
	template<typename T> using D = T;
#	define DL
#	define OP other
#else
	template<typename T> using D = DebugOperand<T>;
#	define DL ((PerfCompiler&)cc).attachDebugLine(other.file, other.line)
#	define OP other.operand
#endif


} // namespace

#endif
