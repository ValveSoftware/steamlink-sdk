#!/bin/bash

source ../../setenv_external.sh

# libffi has an issue with it's pc file so override here
export LIBFFI_LIBS="-L$MARVELL_ROOTFS/usr/lib -lffi"

cp -f marvell.cache config.cache
./configure $STEAMLINK_CONFIGURE_OPTS \
	--cache-file=config.cache \
	--disable-man \
	--disable-gtk-doc-html

steamlink_make_clean
steamlink_make
steamlink_make_install
