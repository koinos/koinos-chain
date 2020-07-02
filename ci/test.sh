#!/bin/bash

set -e
set -x

if [ "$RUN_TYPE" = "test" ]; then
   cd $(dirname "$0")/../build/tests
   exec ctest -j3 --output-on-failure
fi

