#!/bin/bash

source ../../setenv_external.sh

export ZLIB_CFLAGS="-I$MARVELL_ROOTFS/usr/libs"
export ZLIB_LIBS="-lz"

./configure $VALVE_CONFIGURE_OPTS \
	--with-png=no

valve_make_clean
valve_make
valve_make_install
