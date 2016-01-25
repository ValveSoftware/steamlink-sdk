#!/bin/bash

export REG_BIN=regulatory.bin
export USE_OPENSSL=1
export CROSS_TARGET_BITS=32
export V=1
make crda regdbdump
${CROSS_COMPILE}strip crda regdbdump
install -m 755 crda $MARVELL_ROOTFS/usr/bin/crda
install -m 755 regdbdump $MARVELL_ROOTFS/usr/bin/regdbdump
