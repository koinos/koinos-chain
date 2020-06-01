#!/bin/bash

if [ "$TRAVIS_OS_NAME" = "linux" ]; then
   sudo apt-get install -y \
      libboost-all-dev \
      libgflags-dev \
      python3 \
      python3-pip \
      python3-setuptools \
      libsnappy-dev \
      zlib1g-dev \
      libbz2-dev \
      liblz4-dev \
      libzstd-dev \
      valgrind
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
   brew install cmake \
      boost \
      openssl \
      python3 \
      zlib \
      snappy \
      bzip2 \
      gflags \
      lcov
   sudo gem install coveralls-lcov
fi

pip3 install dataclasses-json Jinja2 importlib_resources pluginbase
