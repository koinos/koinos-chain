#!/bin/bash

set -e
set -x

if [[ -z $BUILD_DOCKER ]]; then
   cd $(dirname "$0")/../build/tests
   if [ "$RUN_TYPE" = "test" ]; then
      exec ctest -j3 --output-on-failure && ../libraries/vendor/mira/test/mira_test
   elif [ "$RUN_TYPE" = "coverage" ]; then
      exec valgrind --error-exitcode=1 --leak-check=yes ./koinos_tests
   fi
fi

if ! [[ -z $BUILD_DOCKER ]]; then
   eval "$(gimme 1.18.1)"
   source ~/.gimme/envs/go1.18.1.env

   TAG="$TRAVIS_BRANCH"
   if [ "$TAG" = "master" ]; then
      TAG="latest"
   fi

   export CHAIN_TAG=$TAG

   git clone https://github.com/koinos/koinos-integration-tests.git

   cd koinos-integration-tests
   go get ./...
   ./run.sh
fi
