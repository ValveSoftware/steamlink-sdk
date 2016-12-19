#!/bin/bash

source ../../setenv_external.sh

export LIBS="-ltinfo"

./configure $STEAMLINK_CONFIGURE_OPTS \
	--disable-systemd \
	--with--dbussystembusdir=/var/run/dbus/system_bus_socket \
	--disable-cups \
	--disable-obex \
	--enable-udev \
	--enable-sixaxis

steamlink_make_clean
steamlink_make 
steamlink_make_install
