#!/usr/bin/sudo /usr/bin/bash
#install files.

cmake --build ./build --config Release --target all  && sudo cmake --build ./build --config Release --target install 


