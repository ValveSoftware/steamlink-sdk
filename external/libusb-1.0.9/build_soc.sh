#!/bin/bash

./configure --host=$SOC_BUILD \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--prefix=/usr
make clean && make $MAKE_J && DESTDIR=$MARVELL_ROOTFS make install

