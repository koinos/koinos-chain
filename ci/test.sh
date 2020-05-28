#!/bin/bash

if [ "$RUN_TYPE" = "test" ]; then
   cd build/tests
   ctest -j8 --output-on-failure
fi
