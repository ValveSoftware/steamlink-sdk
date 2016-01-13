#!/bin/bash

source ../../setenv_external.sh

./configure $VALVE_CONFIGURE_OPTS

valve_make_clean
valve_make
valve_make_install

# Libffi pays no attention to '--includedir' and installs ffi.h into /usr/lib/libffi-x.y/include 
mv $MARVELL_ROOTFS/usr/lib/libffi-3.1/include/*.h $MARVELL_ROOTFS/usr/include

