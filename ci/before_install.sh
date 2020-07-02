#!/bin/bash

if [ "$TRAVIS_OS_NAME" = "linux" ]; then
   sudo apt-get update -qq
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
   cd /usr/local/Homebrew/Library/Taps
   if [ -d homebrew/homebrew-cask ]; then rm -rf caskroom/homebrew-cask; fi
   for repo in find -type d -name .git; do
      cd repo/..
      git clean -fxd
   done

   brew cleanup
   brew update
fi
