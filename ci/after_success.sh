#!/bin/bash

if [ "$RUN_TYPE" = "coverage" ]; then
   coveralls-lcov --repo-token "$COVERALLS_REPO_TOKEN" --service-name travis-pro --service-job-id "$TRAVIS_JOB_ID" ./build/coverage.info
fi

