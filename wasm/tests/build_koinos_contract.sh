#!/bin/bash

set -e
set -x

CDT_INSTALL_PATH=/Users/mdv/opt/koinos-cdt
KOINOS_WASI_SDK_ROOT=/Users/mdv/opt/wasi-sdk-12.0

"$KOINOS_WASI_SDK_ROOT/bin/clang++" \
   \
   -v \
   --sysroot="$KOINOS_WASI_SDK_ROOT/share/wasi-sysroot" \
   --target=wasm32-wasi \
   -L$CDT_INSTALL_PATH/lib \
   -I$CDT_INSTALL_PATH/include \
   -L$KOINOS_WASI_SDK_ROOT/share/wasi-sysroot/lib/wasm32-wasi \
   -I$KOINOS_WASI_SDK_ROOT/share/wasi-sysroot/include \
   -lkoinos_proto_embedded \
   -lkoinos_api \
   -lkoinos_api_cpp \
   -lkoinos_wasi_api \
   -Wl,--no-entry \
   -Wl,--allow-undefined \
   \
   -O3 \
   -std=c++17 \
   \
   -o $2 \
   $1
