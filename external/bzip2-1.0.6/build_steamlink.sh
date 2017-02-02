#!/bin/bash

source ../../setenv_external.sh

make clean -f Makefile-libbz2_so
make $MAKE_J -f Makefile-libbz2_so
cp -v bzlib.h "$MARVELL_ROOTFS/usr/include/"
ln -sf libbz2.so.1.0 libbz2.so
cp -v libbz2.so* "$MARVELL_ROOTFS/usr/lib/"
