#!/bin/bash

source ../../setenv_external.sh

cp -f marvell.cache config.cache

./configure $VALVE_CONFIGURE_OPTS \
	--cache=config.cache

valve_make_clean
valve_make
# Readline overrides DESTDIR so remove it from Makefile
sed -i 's:^DESTDIR =$::' Makefile
valve_make_install

