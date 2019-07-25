#!/bin/sh

# run with -j42 to run with 42 parallel tasks

git submodule update --init

pushd external/asmjit
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DASMJIT_BUILD_STATIC=TRUE ..
make $@
popd

pushd external/llvm/llvm
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../install -DLLVM_BUILD_LLVM_DYLIB=ON ..
make $@
make install
popd
