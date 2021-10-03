#!/bin/bash

set -e
set -x

# Run this program manually to generate vm_fixture.hpp
# We should really replace this script with a better, more automated process
koinos_wasm_test_dir=../tests/include/koinos/tests/wasm

mkdir -p $koinos_wasm_test_dir

for file in tests/*.cpp; do
   target_name=$(basename $file .cpp)
   "$KOINOS_WASI_SDK_ROOT/bin/clang++" \
      \
      -v \
      --sysroot="$KOINOS_WASI_SDK_ROOT/share/wasi-sysroot" \
      --target=wasm32-wasi \
      -L$CDT_INSTALL_PATH/lib \
      -I$CDT_INSTALL_PATH/include \
      -L$KOINOS_WASI_SDK_ROOT/share/wasi-sysroot/lib/wasm32-wasi \
      -I$KOINOS_WASI_SDK_ROOT/share/wasi-sysroot/include \
      -lkoinos_api \
      -lkoinos_api_cpp \
      -lkoinos_wasi_api \
      -Wl,--no-entry \
      -Wl,--allow-undefined \
      \
      -O3 \
      -std=c++17 \
      \
      -o $target_name.wasm \
      $file

   xxd -i $target_name.wasm > $koinos_wasm_test_dir/$target_name.hpp
   rm $target_name.wasm
done
