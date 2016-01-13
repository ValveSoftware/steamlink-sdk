#!/bin/bash

#export PKG_CONFIG_PATH=$MARVELL_ROOTFS/usr/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig/
#
#export LIBS="-ltinfo"
#
#export DBUS_CFLAGS="-I$MARVELL_ROOTFS/usr/include/dbus-1.0 \
#	-I$MARVELL_ROOTFS/usr/lib/dbus-1.0/include/"
#
#export GLIB_CFLAGS="-I$MARVELL_ROOTFS/usr/include/glib-2.0/ \
#	-I$MARVELL_ROOTFS/usr/lib/glib-2.0/include/"
#
#./configure --host=$SOC_BUILD \
#	--prefix=/usr \
#	--sysconfdir=/etc \
#	--localstatedir=/var \
#	--without-python \
#	--disable-all-programs \
#	--enable-libblkid \
#	--enable-libuuid
#
#exit 1
#make clean && make $MAKE_J && DESTDIR=$MARVELL_ROOTFS make install

source ../../setenv_external.sh

./configure $VALVE_CONFIGURE_OPTS \
	--without-python \
	--without-ncurses \
	--disable-all-programs \
	--enable-libblkid \
	--enable-libuuid

valve_make_clean
valve_make
valve_make_install
