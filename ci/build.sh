#!/bin/bash

set -e
set -x

if [ "$TRAVIS_OS_NAME" = "osx" ]; then
   PACKAGE_DIR=/usr/local/opt
   export OPENSSL_ROOT_DIR=${PACKAGE_DIR}/openssl@1.1
   export SNAPPY_LIBRARIES=${PACKAGE_DIR}/snappy/lib
   export SNAPPY_INCLUDE_DIR=${PACKAGE_DIR}/snappy/include
   export ZLIB_LIBRARIES=${PACKAGE_DIR}/zlib/lib
fi

mkdir build
cd build

if [ "$RUN_TYPE" = "test" ]; then
   if [ "$TRAVIS_OS_NAME" = "osx" ]; then
      cmake -DCMAKE_BUILD_TYPE=Release -GXcode ..
      cmake --build . --config Release --parallel 3
   else
      cmake -DCMAKE_BUILD_TYPE=Release ..
      cmake --build . --config Release --parallel 3
   fi
elif [ "$RUN_TYPE" = "coverage" ]; then
   cmake -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON ..
   cmake --build . --config Debug --parallel 3 --target coverage
fi
