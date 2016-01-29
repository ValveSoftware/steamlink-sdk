#!/bin/bash

source ../../setenv_external.sh

#./configure $STEAMLINK_CONFIGURE_OPTS

export LIBATTR=no
steamlink_make_clean
steamlink_make
make RAISE_SETFCAP=no DESTDIR=$MARVELL_ROOTFS lib=lib install
