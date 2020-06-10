#!/bin/bash

set -e
set -x

# Run this program manually to generate vm_fixture.hpp
# We should really replace this script with a better, more automated process

mkdir -p ../tests/test_fixtures/wasm
clang --target=wasm32 -Wl,--no-entry -Wl,--export-dynamic -Wl,--allow-undefined -nostartfiles -nodefaultlibs -o hello.wasm hello.c
xxd -i hello.wasm > ../tests/test_fixtures/wasm/hello_wasm.hpp
