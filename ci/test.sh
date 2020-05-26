#!/bin/bash

cd $(dirname "$0")/../build/tests
exec ctest -j8 --output-on-failure
