#!/bin/bash

if [ "$RUN_TYPE" = "coverage" ]; then
   coveralls --gcov coverage.info
fi

