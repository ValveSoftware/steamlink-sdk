#!/bin/bash

source ../../setenv_external.sh

rm -rf build
mkdir build
cd build
MAKE_INSTALL_PREFIX=/usr cmake ..

valve_make_clean
valve_make
valve_make_install
