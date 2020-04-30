#!/bin/bash

if [ "$TRAVIS_OS_NAME" = "osx" ]; then
   export OPENSSL_ROOT_DIR=$(brew --cellar openssl)/$(brew list --versions openssl | tr ' ' '\n' | tail -1)
   export SNAPPY_LIBRARIES=$(brew --cellar snappy)/$(brew list --versions snappy | tr ' ' '\n' | tail -1)/lib
   export SNAPPY_INCLUDE_DIR=$(brew --cellar snappy)/$(brew list --versions snappy | tr ' ' '\n' | tail -1)/include
   export ZLIB_LIBRARIES=$(brew --cellar zlib)/$(brew list --versions zlib | tr ' ' '\n' | tail -1)/lib
fi

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ..
make -j8
