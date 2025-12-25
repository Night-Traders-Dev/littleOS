#!/bin/bash


rm -rf build
mkdir -p build && cd build

cmake .. -DLITTLEOS_ENABLE_USER_ACCOUNT=ON -DLITTLEOS_USER_NAME=appuser
make -j$(nproc)
mv littleos.uf2 ../
cd ../ && rm -rf build
