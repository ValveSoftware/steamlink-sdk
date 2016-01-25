#!/bin/bash

make clean && make && DESTDIR=$MARVELL_ROOTFS BINDIR=/usr/bin LIBDIR=/usr/lib make install
