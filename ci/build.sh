#!/bin/bash

set -e
set -x

if [ "$TRAVIS_OS_NAME" = "osx" ]; then
   export OPENSSL_ROOT_DIR=$(brew --cellar openssl)/$(brew list --versions openssl | tr ' ' '\n' | tail -1)
   export SNAPPY_LIBRARIES=$(brew --cellar snappy)/$(brew list --versions snappy | tr ' ' '\n' | tail -1)/lib
   export SNAPPY_INCLUDE_DIR=$(brew --cellar snappy)/$(brew list --versions snappy | tr ' ' '\n' | tail -1)/include
   export ZLIB_LIBRARIES=$(brew --cellar zlib)/$(brew list --versions zlib | tr ' ' '\n' | tail -1)/lib
fi

mkdir build
cd build

if [ "$RUN_TYPE" = "test" ]; then
   if [ "$TRAVIS_OS_NAME" = "osx" ]; then
      cmake -DCMAKE_BUILD_TYPE=Release -GXcode ..
   else
      cmake -DCMAKE_BUILD_TYPE=Release ..
   fi
   cmake --build . --config Release --parallel 3
elif [ "$RUN_TYPE" = "coverage" ]; then
   cmake -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON ..
   cmake --build . --config Debug --parallel 3 --target coverage
fi
