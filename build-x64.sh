#!/bin/bash
# Run a CMake build.
# configure for release (AVX optimizations only. See build-march-x64.sh for multi-arch build with both default and AVX optimizations)

set -e

# clean build
rm -rf build

mkdir -p build
cd build
cmake .. -D CMAKE_BUILD_TYPE=Release -D TOOB_AMD_OPTIMIZATIONS=AVX -D TOOB_MULTI_ARCH_BUILD=OFF   -D CMAKE_VERBOSE_MAKEFILE=ON -G Ninja 
cd ..

time cmake --build ./build --target all  --config Release -- -j 3

./makePackage.sh

