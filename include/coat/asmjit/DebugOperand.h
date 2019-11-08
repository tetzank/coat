#ifndef COAT_ASMJIT_DEBUGOPERAND_H_
#define COAT_ASMJIT_DEBUGOPERAND_H_


namespace coat {

#ifdef PROFILING_SOURCE
	// it just carries the source file and line number, additionally to the operand
	template<typename T>
	struct DebugOperand {
		const T &operand;
		const char *file;
		int line;

		DebugOperand(const T &operand, const char *file=__builtin_FILE(), int line=__builtin_LINE())
			: operand(operand), file(file), line(line) {}
	};

	template<typename T> using D = DebugOperand<T>;
#	define DL ((PerfCompiler&)cc).attachDebugLine(other.file, other.line)
#	define OP other.operand
#	define PASSOP(x) {(x), other.file, other.line}
#else
	template<typename T> using D = T;
#	define DL
#	define OP other
#	define PASSOP(x) (x)
#endif


} // namespace

#endif
