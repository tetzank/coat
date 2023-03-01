cmake_minimum_required(VERSION 3.12)

# Build type defaults to release
if(NOT DEFINED CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS) Debug Release RelWithdebInfo MinSizeRel.")
endif()

project(coat CXX)

option(ENABLE_ASMJIT "enable AsmJit backend" ON)
option(ENABLE_LLVMJIT "enable LLVM JIT backend" ON)

enable_testing()

macro(coat_add_executable)
	set(options TEST)
	set(oneValueArgs BACKEND)
	set(multiValueArgs NAMES)
	cmake_parse_arguments(COAT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

	foreach(NAME ${COAT_NAMES})
		if (COAT_TEST)
			set(TARGET test_${PROJECT_NAME}_${COAT_BACKEND}_${NAME})
			set(SRC src/tests/${NAME}.cpp)
		else()
			set(TARGET example_${PROJECT_NAME}_${COAT_BACKEND}_${NAME})
			set(SRC src/examples/${NAME}.cpp)
		endif()
		add_executable(${TARGET} ${SRC})
		set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
		target_link_libraries(${TARGET} ${PROJECT_NAME}::${COAT_BACKEND})
		if (COAT_TEST)
			add_test(${TARGET} ${TARGET})
		endif()
	endforeach()
endmacro()
     
if (ENABLE_LLVMJIT)
	find_package(LLVM CONFIG REQUIRED PATHS "/usr/lib/llvm-12/lib/cmake" NO_DEFAULT_PATH)
	find_package(Clang CONFIG REQUIRED PATHS "/usr/lib/llvm-12/lib/cmake" NO_DEFAULT_PATH)

	if(MSVC)
		set(DISABLE_RTTI_FLAG /GR-)
	else()
		set(DISABLE_RTTI_FLAG -fno-rtti)
	endif()

	add_library(${PROJECT_NAME}_llvmjit INTERFACE)
	set_target_properties(${PROJECT_NAME}_llvmjit PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
	target_include_directories(${PROJECT_NAME}_llvmjit INTERFACE ${CMAKE_SOURCE_DIR}/include)
	add_library(${PROJECT_NAME}::llvmjit ALIAS ${PROJECT_NAME}_llvmjit)

	separate_arguments(LLVM_DEFINITIONS)
	target_compile_definitions(${PROJECT_NAME}_llvmjit INTERFACE ${LLVM_DEFINITIONS})

	target_include_directories(${PROJECT_NAME}_llvmjit INTERFACE ${LLVM_INCLUDE_DIRS})
	if(NOT LLVM_ENABLE_RTTI)
		target_compile_options(${PROJECT_NAME}_llvmjit INTERFACE ${DISABLE_RTTI_FLAG})
	endif()

	target_compile_definitions(${PROJECT_NAME}_llvmjit INTERFACE ENABLE_LLVMJIT)
	if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		target_compile_definitions(${PROJECT_NAME}_llvmjit INTERFACE LLVMJIT_DEBUG)
	endif()
	target_link_directories(${PROJECT_NAME}_llvmjit INTERFACE ${LLVM_LIBRARY_DIRS})
	target_link_libraries(${PROJECT_NAME}_llvmjit INTERFACE LLVMCore LLVMX86CodeGen LLVMPerfJITEvents LLVMOrcJIT LLVMExecutionEngine)

	coat_add_executable(NAMES
		tests
		calculator
		fibonacci
		BACKEND llvmjit)

	coat_add_executable(NAMES
		sum
		cast
		call
		reading_datastruct
		# Just trying vector types
		vectormean
		vectormerge
		vectorintersection
		BACKEND llvmjit
		TEST)

endif()

if(ENABLE_ASMJIT)
	add_subdirectory(ThirdParty/asmjit)
	set(ASMJIT_INCLUDE_DIR ThirdParty/asmjit/src/asmjit)
	set(ASMJIT_LIBRARIES asmjit)
	add_subdirectory(ThirdParty/asmjit-utilities/perf)

	add_library(${PROJECT_NAME}_asmjit INTERFACE)
	set_target_properties(${PROJECT_NAME}_asmjit PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
	target_include_directories(${PROJECT_NAME}_asmjit INTERFACE ${CMAKE_SOURCE_DIR}/include)
	add_library(${PROJECT_NAME}::asmjit ALIAS ${PROJECT_NAME}_asmjit)
	
    target_compile_definitions(${PROJECT_NAME}_asmjit INTERFACE ENABLE_ASMJIT)
    target_link_libraries(${PROJECT_NAME}_asmjit INTERFACE asmjit asmjit_perf)

	# Debugging options
	if(NOT DEFINED PROFILING)
		set(PROFILING "" CACHE STRING "Choose type of profiling support for perf and AsmJit, options are: None, Assembly, Source")
	endif()
	if(PROFILING STREQUAL "Assembly")
		target_compile_definitions(${PROJECT_NAME}_asmjit INTERFACE PROFILING_ASSEMBLY)
	elseif(PROFILING STREQUAL "Source")
		target_compile_definitions(${PROJECT_NAME}_asmjit PROFILING_SOURCE)
	endif()

	coat_add_executable(NAMES
		tests
		calculator
		fibonacci
		BACKEND asmjit)

	coat_add_executable(NAMES
		sum
		cast
		call
		reading_datastruct
		# Just trying vector types
		vectormean
		BACKEND asmjit
		TEST)

endif()

