#!/bin/bash

set -e
set -x

if [[ -z $BUILD_DOCKER ]]; then
   if [ "$RUN_TYPE" = "test" ]; then
      cd $(dirname "$0")/../build/tests
      exec ctest -j3 --output-on-failure && ../libraries/vendor/mira/test/mira_test
   fi
fi

if ! [[ -z $BUILD_DOCKER ]]; then
   TAG="$TRAVIS_BRANCH"
   if [ "$TAG" = "master" ]; then
      TAG="latest"
   fi

   export CHAIN_TAG=$TAG

   git clone https://github.com/koinos/koinos-integration-tests.git

   cd koinos-integration-tests
   go get ./...
   cd tests
   ./run.sh
fi
