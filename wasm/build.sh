#!/bin/bash

set -e
set -x

# Run this program manually to generate vm_fixture.hpp
# We should really replace this script with a better, more automated process
koinos_wasm_test_dir=../tests/include/koinos/tests/wasm

mkdir -p $koinos_wasm_test_dir

for file in tests/*.c; do
   target_name=$(basename $file .c)
   clang --target=wasm32 -Wl,--no-entry -Wl,--export-dynamic -Wl,--allow-undefined -nostartfiles -nodefaultlibs -o $target_name.wasm $file
   xxd -i $target_name.wasm > $koinos_wasm_test_dir/$target_name.hpp
   rm $target_name.wasm
done
