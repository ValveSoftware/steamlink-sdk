#!/bin/bash

./configure --host=$SOC_BUILD \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--prefix=/usr \
	--without-systemdsystemunitdir \
	--disable-systemd \
	--disable-static \
	--disable-tests

steamlink_make_clean
steamlink_make
steamlink_make_install
