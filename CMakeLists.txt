cmake_minimum_required(VERSION 3.12)

if(NOT DEFINED CMAKE_CXX_FLAGS)
	set(CMAKE_CXX_FLAGS "-Wall -Wextra -march=native -fno-rtti" CACHE STRING "Flags used by the compiler during all build types.")
endif()
# build type defaults to release
if(NOT DEFINED CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS) Debug Release RelWithdebInfo MinSizeRel.")
endif()

project(dangerousAssembly CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("${CMAKE_SOURCE_DIR}/include/")

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


option(ENABLE_ASMJIT "enable AsmJit backend" ON)
option(ENABLE_LLVMJIT "enable LLVM JIT backend" ON)


add_executable(test test.cpp)
if(ENABLE_ASMJIT)
	target_compile_definitions(test PRIVATE "ENABLE_ASMJIT")
	target_link_libraries(test ${ASMJIT_LIBRARIES})
endif()
if(ENABLE_LLVMJIT)
	target_compile_definitions(test PRIVATE "ENABLE_LLVMJIT")
	target_link_libraries(test ${LLVM_LIBRARIES})
endif()


add_executable(filter filter.cpp)
add_executable(equijoin equijoin.cpp)
add_executable(equijoin_unique equijoin_unique.cpp)
add_executable(semijoin semijoin.cpp)
if(ENABLE_ASMJIT)
	target_compile_definitions(filter PRIVATE "ENABLE_ASMJIT")
	target_link_libraries(filter ${ASMJIT_LIBRARIES})
	target_compile_definitions(equijoin PRIVATE "ENABLE_ASMJIT")
	target_link_libraries(equijoin ${ASMJIT_LIBRARIES})
	target_compile_definitions(equijoin_unique PRIVATE "ENABLE_ASMJIT")
	target_link_libraries(equijoin_unique ${ASMJIT_LIBRARIES})
	target_compile_definitions(semijoin PRIVATE "ENABLE_ASMJIT")
	target_link_libraries(semijoin ${ASMJIT_LIBRARIES})
endif()
if(ENABLE_LLVMJIT)
	target_compile_definitions(filter PRIVATE "ENABLE_LLVMJIT")
	target_link_libraries(filter ${LLVM_LIBRARIES})
	target_compile_definitions(equijoin PRIVATE "ENABLE_LLVMJIT")
	target_link_libraries(equijoin ${LLVM_LIBRARIES})
	target_compile_definitions(equijoin_unique PRIVATE "ENABLE_LLVMJIT")
	target_link_libraries(equijoin_unique ${LLVM_LIBRARIES})
	target_compile_definitions(semijoin PRIVATE "ENABLE_LLVMJIT")
	target_link_libraries(semijoin ${LLVM_LIBRARIES})
endif()
