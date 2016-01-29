#!/bin/sh

export ARCH=arm
export CROSS_COMPILE=armv7a-cros-linux-gnueabi-
export CC="armv7a-cros-linux-gnueabi-gcc --sysroot=$MARVELL_ROOTFS -marm -mfloat-abi=hard"
export V=1
make

