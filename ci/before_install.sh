#!/bin/bash

if [ "$TRAVIS_OS_NAME" = "linux" ]; then
   sudo apt-get update -qq
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
   brew update
fi
