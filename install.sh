#!/bin/sh
#install files.
# Beware! building "--target all" while sudo makes a mess of permissions in the build directory.
sudo cmake --build ./build --config Release --target all  && sudo cmake --build ./build --config Release --target install 


