#!/bin/sh
# Configure CMake build.
mkdir build-a76
cd build-a76
cmake .. -D CMAKE_BUILD_TYPE=Release  -D A76_OPTIMIZATION=ON $@
cd ..
