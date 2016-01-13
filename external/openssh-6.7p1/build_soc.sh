#!/bin/bash

source ../../setenv_external.sh

./configure $VALVE_CONFIGURE_OPTS
patch -p0 < 01-fix-check-config.patch

valve_make_clean
valve_make
export STRIPPROG=armv7a-cros-linux-gnueabi-strip
make DESTDIR=$MARVELL_ROOTFS STRIP_OPT="-s --strip-program=${STRIPPROG}" install

