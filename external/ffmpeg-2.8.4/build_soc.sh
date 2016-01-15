#!/bin/bash

source ../../setenv_external.sh

# libpostproc, which is needed by Kodi, requires --enable-gpl 
export as_type=gcc
./configure --enable-cross-compile --arch=arm --target-os=linux --cc="$CC" --cxx="$CXX" --dep-cc="$CC" --ld="$CC" --as="$CC" --prefix=/usr --enable-gpl --disable-programs --disable-doc || exit 1

valve_make_clean
valve_make
valve_make_install
