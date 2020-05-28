#!/bin/bash

if [ "$RUN_TYPE" = "test" ]; then
   cd $(dirname "$0")/../build/tests
   exec ctest -j8 --output-on-failure
fi

