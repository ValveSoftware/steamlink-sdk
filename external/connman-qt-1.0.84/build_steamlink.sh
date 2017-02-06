#!/bin/bash

source ../../setenv_external.sh

qmake || exit 1

steamlink_make_clean
steamlink_make
pushd libconnman-qt
make INSTALL_ROOT=$MARVELL_ROOTFS install
popd
