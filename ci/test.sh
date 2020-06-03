#!/bin/bash

set -e
set -x

if [ "$RUN_TYPE" = "test" ]; then

   SRC_DIR=$(realpath $(dirname "$0")/..)
   BUILD_DIR="$SRC_DIR/build"

   # In CI, vmtest is guaranteed to exist if we've gotten to this point (compilation failure bails in build.sh)
   # So the existence check is only applicable when running this script manually
   if [ -e "$BUILD_DIR/programs/koinos-vm/koinos-vm" ]
   then
      cd "$SRC_DIR/programs/koinos-vm"
      "$BUILD_DIR/programs/koinos-vm/koinos-vm"
   fi

   cd "$BUILD_DIR/tests"
   ctest -j8 --output-on-failure
fi

