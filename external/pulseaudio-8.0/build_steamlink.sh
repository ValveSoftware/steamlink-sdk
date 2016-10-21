#!/bin/bash

source ../../setenv_external.sh

./configure $STEAMLINK_CONFIGURE_OPTS \
	  --disable-oss-output \
	  --disable-oss-wrapper \
	  --disable-coreaudio-output \
	  --disable-systemd-daemon \
	  --disable-manpages \
	  --disable-bluez4 \
	  --disable-bluez5-ofono-headset \
	  --enable-bluez5 \
	  --disable-udev \
	  --disable-alsa

steamlink_make_clean
steamlink_make
steamlink_make_install
