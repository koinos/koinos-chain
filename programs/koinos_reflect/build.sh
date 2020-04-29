#!/bin/bash

set -e
set -x

rm -Rf build
mkdir -p build
python3 -m koinos_reflect.analyze block.bt types.hpp block.hpp -s -o build/block.schema
python3 -m koinos_codegen.codegen --target-path lang -o build -p koinos_protocol build/block.schema

cp tester.py build
cd build
python3 tester.py
