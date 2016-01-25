#!/bin/bash

source ../../setenv_external.sh

cp -f marvell.cache config.cache

./configure $STEAMLINK_CONFIGURE_OPTS \
	--cache=config.cache

steamlink_make_clean
steamlink_make
# Readline overrides DESTDIR so remove it from Makefile
sed -i 's:^DESTDIR =$::' Makefile
steamlink_make_install

