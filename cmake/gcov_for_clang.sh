#!/bin/sh

which llvm-cov-11 > /dev/null
if [ $? -eq 0 ]; then
   exec llvm-cov-11 gcov "$@"
else
   exec llvm-cov gcov "$@"
fi
