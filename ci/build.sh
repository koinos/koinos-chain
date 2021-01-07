#!/bin/bash

set -e
set -x

mkdir build
cd build

if [ "$RUN_TYPE" = "test" ]; then
   cmake -DCMAKE_BUILD_TYPE=Release -DHUNTER_DISABLE_BUILDS=ON ..
   cmake --build . --config Release --parallel 3
elif [ "$RUN_TYPE" = "coverage" ]; then
   cmake -DCMAKE_BUILD_TYPE=Debug -DHUNTER_DISABLE_BUILDS=ON -DCOVERAGE=ON ..
   cmake --build . --config Debug --parallel 3 --target coverage
fi
