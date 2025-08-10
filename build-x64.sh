#!/bin/sh
# Run a CMake build.

set -e
rm -rf build

# Build for ToobAmp-a76.so
mkdir -p build
cd build
cmake .. -D CMAKE_BUILD_TYPE=Release  -D CMAKE_VERBOSE_MAKEFILE=ON $@
cd ..

cmake --build ./build --target all  --config Release 

