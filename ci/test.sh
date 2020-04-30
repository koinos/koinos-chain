#!/bin/bash

cd build/tests
ctest -j8 --output-on-failure
