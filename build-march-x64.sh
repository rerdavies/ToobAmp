#!/bin/bash
# Build aarch64 x86_64 + x86_64_avx package for production.
set -e
rm -rf build
rm -rf build-avx

# Build for ToobAmp-a76.so
mkdir -p build-avx
cd build-avx
cmake .. -D CMAKE_BUILD_TYPE=Release  -D TOOB_AMD_OPTIMIZATIONS=AVX -D TOOB_MULTI_ARCH_BUILD=OFF -D CMAKE_VERBOSE_MAKEFILE=ON -G Ninja 
cd ..

# Build only the x86-64-avx version of the library, to be used in the multi-arch build.
time cmake --build ./build-avx --target ToobAmpArch  --config Release -- -j 6


# Configure main build to do an a72 build and an a72+a76 package (a76 binaries are in ./build-a76)
echo Configuring MARCH Build
mkdir -p build
cd build
time cmake .. -D CMAKE_BUILD_TYPE=Release  -D TOOB_AMD_OPTIMIZATIONS=DEFAULT -D TOOB_MULTI_ARCH_BUILD=ON -D CMAKE_VERBOSE_MAKEFILE=ON -G Ninja  
cd ..
echo Run MARCH Build
time cmake --build ./build --target all --config Release -- -j 6



# build the package.
./makePackage.sh



#restore main build options.
# Configure main build to do an a72+a76 build
#mkdir -p build
#cd build
#cmake .. -D CMAKE_BUILD_TYPE=Release  -D TOOBAMP_MULTI_ARCH_BUILD=OFF -D CMAKE_VERBOSE_MAKEFILE=ON $@
#cd ..

