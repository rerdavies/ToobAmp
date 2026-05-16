#!/bin/bash
# Build aarch64 A72+A76 package for production.
set -e
rm -rf build
rm -rf build-a76

# Build for ToobAmp-a76.so
mkdir -p build-a76
cd build-a76
cmake .. -D CMAKE_BUILD_TYPE=Release  -D TOOBAMP_A76_OPTIMIZATION=ON -D TOOB_MULTI_ARCH_BUILD=OFF -D CMAKE_VERBOSE_MAKEFILE=ON -G Ninja 
cd ..

# Build only the A76 version of the librar, to be used in the multi-arch build.
time cmake --build ./build-a76 --target ToobAmpArch  --config Release -G Ninja


# Configure main build to do an a72+a76 build
mkdir -p build
cd build
time cmake .. -D CMAKE_BUILD_TYPE=Release  -D TOOBAMP_A72_OPTIMIZATION=ON -D TOOB_MULTI_ARCH_BUILD=ON -D CMAKE_VERBOSE_MAKEFILE=ON -G Ninja  
cd ..

time cmake --build ./build --target all --config Release -- -j 6



# build the package.
    ./makePackage.sh



#restore main build options.
# Configure main build to do an a72+a76 build
#mkdir -p build
#cd build
#cmake .. -D CMAKE_BUILD_TYPE=Release  -D TOOBAMP_MULTI_ARCH_BUILD=OFF -D CMAKE_VERBOSE_MAKEFILE=ON $@
#cd ..

