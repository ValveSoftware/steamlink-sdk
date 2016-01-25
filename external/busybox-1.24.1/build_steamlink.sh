#!/bin/bash

source ../../setenv_external.sh

make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE steamlink_defconfig
make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE busybox

valve_make_clean && valve_make 
install -m 755 busybox $MARVELL_SDK_PATH/MRVL/MV88DE3100_SDK/MV88DE3100_Tools/recovery-initramfs/bin/busybox
install -m 755 busybox $MARVELL_ROOTFS/bin/busybox
