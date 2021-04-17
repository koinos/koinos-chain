#!/bin/bash

if [[ -z $BUILD_DOCKER ]]; then
   if [ "$CC" = "clang-11" ]; then
      wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key|sudo apt-key add -
      sudo apt-add-repository 'deb http://apt.llvm.org/focal/ llvm-toolchain-focal-11 main'
      sudo apt-get install -y llvm-11 clang-11
   fi

   if [ "$RUN_TYPE" = "coverage" ]; then
      sudo apt-get install -y lcov ruby
      sudo gem install coveralls-lcov
   fi

   pip3 install --user dataclasses_json Jinja2 importlib_resources pluginbase gitpython
else
   sudo curl -L "https://github.com/docker/compose/releases/download/1.29.1/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
   sudo chmod +x /usr/local/bin/docker-compose
   docker-compose --version
fi
