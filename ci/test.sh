#!/bin/bash

set -e
set -x

SRC_DIR=$(realpath $(dirname "$0")/..)
BUILD_DIR="$SRC_DIR/build"

# In CI, vmtest is guaranteed to exist if we've gotten to this point (compilation failure bails in build.sh)
# So the existence check is only applicable when running this script manually
if [ -e "$BUILD_DIR/programs/vmtest/vmtest" ]
then
   cd "$SRC_DIR/programs/vmtest"
   "$BUILD_DIR/programs/vmtest/vmtest"
fi

cd "$BUILD_DIR/tests"
ctest -j8 --output-on-failure
