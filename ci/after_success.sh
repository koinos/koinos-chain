#!/bin/bash

if [ "$RUN_TYPE" = "coverage" ]; then
   # coveralls -n -t $COVERALLS_REPO_TOKEN --gcov ./build/coverage.info
   coveralls-lcov --repo-token "$COVERALLS_REPO_TOKEN" --service-name travis-pro ./build/coverage.info
fi

