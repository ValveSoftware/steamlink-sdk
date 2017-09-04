#!/bin/sh

./configure -xplatform macx-ios-clang -release -opensource -confirm-license -nomake examples -nomake tests || exit 1
make && make install
