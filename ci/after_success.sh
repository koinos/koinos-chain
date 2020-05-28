#!/bin/bash

if [ "$RUN_TYPE" = "coverage" ]; then
   coveralls -n -t $COVERALLS_REPO_TOKEN --gcov ./build/coverage.info
fi

