#!/bin/bash

set -e

# This scrips is used to build the multi-arch (a72 + a76) package for production.

# Build for ToobAmp-a76.so
mkdir -p build-a76
cd build-a76
cmake .. -D CMAKE_BUILD_TYPE=RelWithDebInfo  -D A76_OPTIMIZATION=ON -D TOOB_MULTI_ARCH_BUILD=OFF $@
cd ..

cmake --build ./build-a76 --config RelWithDebInfo --target ToobAmpArch


# Configure main build to do an a72+a76 build
mkdir -p build
cd build
cmake .. -D CMAKE_BUILD_TYPE=RelWithDebInfo  -D TOOB_MULTI_ARCH_BUILD=ON $@
cd ..

cmake --build ./build --config RelWithDebInfo --target all 

#restore main build options.
# Configure main build to do an a72+a76 build
mkdir -p build
cd build
cmake .. -D CMAKE_BUILD_TYPE=RelWithDebInfo  $@
cd ..


# build the package.
./makePackage.sh



