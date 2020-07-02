#!/bin/bash

if [ "$TRAVIS_OS_NAME" = "linux" ]; then
   sudo apt-get update -qq
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
   source ci/travis_osx_brew_cache.sh
   TAPS="$(brew --repository)/Library/Taps"
   if [ -e "$TAPS/caskroom/homebrew-cask" -a -e "$TAPS/homebrew/homebrew-cask" ]; then
         rm -rf "$TAPS/caskroom/homebrew-cask"
   fi
   find "$TAPS" -type d -name .git -exec \
            bash -xec '
               cd $(dirname '\''{}'\'') || echo "status: $?"
               git clean -fxd || echo "status: $?"
               sleep 1 || echo "status: $?"
               git status || echo "status: $?"' \; || echo "status: $?"
   brew_cache_cleanup
fi
