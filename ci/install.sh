#!/bin/bash

if [[ -z $BUILD_DOCKER ]]; then
   sudo apt-get install -yq --allow-downgrades libc6=2.31-0ubuntu9.2 libc6-dev=2.31-0ubuntu9.2
   sudo -E apt-get -yq --no-install-suggests --no-install-recommends --allow-downgrades --allow-remove-essential --allow-change-held-packages install clang-11 llvm-11 -o Debug::pkgProblemResolver=yes

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
