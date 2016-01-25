#!/bin/bash

source ../../setenv_external.sh

./configure $STEAMLINK_CONFIGURE_OPTS \
	--without-tests \
	--with-termlib=tinfo \
	--without-manpages \
	--with-install-prefix=$MARVELL_ROOTFS


steamlink_make_clean
steamlink_make
steamlink_make_install
