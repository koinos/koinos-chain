#!/bin/bash

set -e
set -x

if [[ -z $BUILD_DOCKER ]]; then
   mkdir build
   cd build

   if [ "$RUN_TYPE" = "test" ]; then
      cmake -DCMAKE_BUILD_TYPE=Release ..
      cmake --build . --config Release --parallel 3
   elif [ "$RUN_TYPE" = "coverage" ]; then
      cmake -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON ..
      cmake --build . --config Debug --parallel 3 --target coverage
   fi
else
   cp -R ~/.ccache ./.ccache
   docker build . -t koinos-chain-ccache --target builder
   docker build . -t koinos-chain
   docker run -td --name ccache koinos-chain-ccache
   docker cp ccache:/koinos-chain/.ccache ~/
fi
