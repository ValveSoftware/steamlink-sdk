#!/bin/bash

source ../../setenv_external.sh

export LIBS="-ltinfo"

./configure $VALVE_CONFIGURE_OPTS \
	--disable-systemd \
	--with--dbussystembusdir=/var/run/dbus/system_bus_socket \
	--disable-cups \
	--disable-udev \
	--disable-obex 

valve_make_clean && valve_make 
valve_make_install
