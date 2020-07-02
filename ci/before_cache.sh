#!/bin/bash

if [ "$TRAVIS_OS_NAME" = "osx" ]; then
   ./travis_osx_brew_cache.sh
   set -e; set -x
   if [ -n "$IS_OSX" ]; then
         # When Taps is cached, this dir causes "Error: file exists" on `brew update`
         if [ -e "$(brew --repository)/Library/Taps/homebrew/homebrew-cask/homebrew-cask" ]; then
            rm -rf "$(brew --repository)/Library/Taps/homebrew/homebrew-cask/homebrew-cask"
         fi
         brew_cache_cleanup
   fi
   set +x; set +e
fi
