#!/bin/sh
# Run a CMake build.


cmake --build ./build-a76 --config Release -D TOOB_AARCH_OPTIMIZATIONS=A76 --target ToobAmpArch -- -j 3
