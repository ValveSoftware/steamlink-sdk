#!/bin/bash

source ../../setenv_external.sh

./configure $VALVE_CONFIGURE_OPTS \
	--without-tests \
	--with-termlib=tinfo \
	--without-manpages \
	--with-install-prefix=$MARVELL_ROOTFS


valve_make_clean
valve_make
valve_make_install
