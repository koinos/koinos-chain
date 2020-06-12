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
      valgrind \
      ccache
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
   brew install cmake \
      boost \
      openssl \
      zlib \
      snappy \
      bzip2 \
      gflags \
      ccache

   if [ "$RUN_TYPE" = "coverage" ]; then
      brew install lcov
      sudo gem install coveralls-lcov
   fi
fi

pip3 install --user dataclasses-json Jinja2 importlib_resources pluginbase
