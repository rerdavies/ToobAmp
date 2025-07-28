#!/bin/sh
# Configure CMake build.
mkdir build
cd build
cmake .. -D CMAKE_BUILD_TYPE=RelWithDebInfo  $@
cd ..

cmake --build ./build --config RelWithDebInfo --target all 


cmake --build ./build-a76 --config RelWithDebInfo --target ToobAmp-a76.so
