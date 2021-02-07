cmake_minimum_required(VERSION 3.12)

if(NOT DEFINED CMAKE_CXX_FLAGS)
	set(CMAKE_CXX_FLAGS "-Wall -Wextra -march=native -fno-rtti" CACHE STRING "Flags used by the compiler during all build types.")
endif()
# build type defaults to release
if(NOT DEFINED CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS) Debug Release RelWithdebInfo MinSizeRel.")
endif()

project(dangerousAssembly CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("${CMAKE_SOURCE_DIR}/include/")

option(ENABLE_ASMJIT "enable AsmJit backend" ON)
option(ENABLE_LLVMJIT "enable LLVM JIT backend" ON)

if(ENABLE_ASMJIT)
	# find asmjit
	find_path(ASMJIT_INCLUDE_DIR
		asmjit/asmjit.h
		HINTS external/asmjit ENV ASMJIT_ROOT
		PATH_SUFFIXES src
	)
	find_library(ASMJIT_LIBRARIES
		asmjit
		HINTS external/asmjit ENV ASMJIT_ROOT
		PATH_SUFFIXES build build_next
	)
	include_directories(${ASMJIT_INCLUDE_DIR})

	# find asmjit_perf
	find_path(ASMJIT_PERF_INCLUDE_DIR
		asmjit-utilities/perf/jitdump.h
		HINTS external/
	)
	find_library(ASMJIT_PERF_LIBRARIES
		asmjit_perf
		HINTS external/asmjit-utilities
		PATH_SUFFIXES perf/build
	)
	include_directories(${ASMJIT_PERF_INCLUDE_DIR})
endif()

if(ENABLE_LLVMJIT)
	# find llvm, quick&dirty
	find_path(LLVM_INCLUDE_DIR
		llvm/IR/IRBuilder.h
		HINTS external/llvm/llvm/install ENV LLVM_ROOT
		PATH_SUFFIXES include
	)
	find_library(LLVM_LIBRARIES
		LLVM-7.1
		HINTS external/llvm/llvm/install ENV LLVM_ROOT
		PATH_SUFFIXES lib build/lib
	)
	include_directories(${LLVM_INCLUDE_DIR})
endif()


# debugging options
if(NOT DEFINED PROFILING)
	set(PROFILING "" CACHE STRING "Choose type of profiling support for perf and AsmJit, options are: None, Assembly, Source")
endif()
if(PROFILING STREQUAL "Assembly")
	add_compile_definitions("PROFILING_ASSEMBLY")
elseif(PROFILING STREQUAL "Source")
	add_compile_definitions("PROFILING_SOURCE")
endif()

add_executable(tests tests.cpp)
if(ENABLE_ASMJIT)
	target_compile_definitions(tests PRIVATE "ENABLE_ASMJIT")
	target_link_libraries(tests ${ASMJIT_LIBRARIES} ${ASMJIT_PERF_LIBRARIES})
endif()
if(ENABLE_LLVMJIT)
	target_compile_definitions(tests PRIVATE "ENABLE_LLVMJIT")
	target_link_libraries(tests ${LLVM_LIBRARIES})
endif()

enable_testing()
add_subdirectory(${CMAKE_SOURCE_DIR}/examples)

add_executable(calculator calculator.cpp)
if(ENABLE_ASMJIT)
	target_compile_definitions(calculator PRIVATE "ENABLE_ASMJIT")
	target_link_libraries(calculator ${ASMJIT_LIBRARIES} ${ASMJIT_PERF_LIBRARIES})
endif()
if(ENABLE_LLVMJIT)
	target_compile_definitions(calculator PRIVATE "ENABLE_LLVMJIT")
	target_link_libraries(calculator ${LLVM_LIBRARIES})
endif()

add_executable(fibonacci fibonacci.cpp)
if(ENABLE_ASMJIT)
	target_compile_definitions(fibonacci PRIVATE "ENABLE_ASMJIT")
	target_link_libraries(fibonacci ${ASMJIT_LIBRARIES} ${ASMJIT_PERF_LIBRARIES})
endif()
if(ENABLE_LLVMJIT)
	target_compile_definitions(fibonacci PRIVATE "ENABLE_LLVMJIT")
	target_link_libraries(fibonacci ${LLVM_LIBRARIES})
endif()
