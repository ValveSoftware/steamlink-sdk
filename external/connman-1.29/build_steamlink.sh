#!/bin/bash

source ../../setenv_external.sh

export LIBS="-ltinfo"

export DBUS_CFLAGS="-I$MARVELL_ROOTFS/usr/include/dbus-1.0 \
	-I$MARVELL_ROOTFS/usr/lib/dbus-1.0/include/"

export GLIB_CFLAGS="-I$MARVELL_ROOTFS/usr/include/glib-2.0/ \
	-I$MARVELL_ROOTFS/usr/lib/glib-2.0/include/"

./configure $STEAMLINK_CONFIGURE_OPTS \
	--disable-polkit \
	--disable-selinux \
	--disable-gadget \
	--disable-bluetooth \
	--disable-ofono \
	--disable-tools \
	--disable-test

steamlink_make_clean
steamlink_make
steamlink_make_install

install -m 755 client/connmanctl $MARVELL_ROOTFS/usr/bin
