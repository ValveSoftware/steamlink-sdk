#!/bin/bash

source ../../setenv_external.sh
export UDEV_CFLAGS="-I${MARVELL_ROOTFS}/usr/include"
export UDEV_LIBS="-L${MARVELL_ROOTFS}/usr/lib -ludev"

./configure $STEAMLINK_CONFIGURE_OPTS \
	  --disable-oss-output \
	  --disable-oss-wrapper \
	  --disable-coreaudio-output \
	  --disable-systemd-daemon \
	  --disable-manpages \
	  --disable-bluez4 \
	  --disable-bluez5-ofono-headset \
	  --enable-bluez5 \
	  --enable-udev


steamlink_make_clean
steamlink_make
steamlink_make_install
