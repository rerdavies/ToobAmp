#!/bin/sh
#install files.

cmake --build ./build --config Release --target all  && sudo cmake --build ./build --config Release --target install 


