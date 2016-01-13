#!/bin/bash

source ../../setenv_external.sh

../qt-everywhere-opensource-src-5.4.1/build/host/bin/qmake || exit 1

valve_make_clean
valve_make
pushd libconnman-qt
make INSTALL_ROOT=$MARVELL_ROOTFS install
popd
