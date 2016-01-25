#!/bin/bash

source ../../setenv_external.sh

#./configure $VALVE_CONFIGURE_OPTS

export LIBATTR=no
valve_make_clean && valve_make && make RAISE_SETFCAP=no DESTDIR=$MARVELL_ROOTFS lib=lib install
