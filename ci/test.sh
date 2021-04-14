#!/bin/bash

set -e
set -x

if [[ -z $BUILD_DOCKER ]]; then
   if [ "$RUN_TYPE" = "test" ]; then
      cd $(dirname "$0")/../build/tests
      exec ctest -j3 --output-on-failure && ../libraries/vendor/mira/test/mira_test
   fi
fi
