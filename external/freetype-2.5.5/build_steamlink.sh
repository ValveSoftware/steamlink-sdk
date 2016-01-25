#!/bin/bash

source ../../setenv_external.sh

export ZLIB_CFLAGS="-I$MARVELL_ROOTFS/usr/libs"
export ZLIB_LIBS="-lz"

./configure $STEAMLINK_CONFIGURE_OPTS \
	--with-png=no

steamlink_make_clean
steamlink_make
steamlink_make_install
