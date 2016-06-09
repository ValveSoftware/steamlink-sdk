#!/bin/sh

mkdir -p /mnt/factory
mountpoint /mnt/factory
if [ ! $? = 0 ]; then
        mount_part -oro factory /mnt/factory
        #mount -o remount,ro /mnt/factory
else
        echo "factory partition is already mounted, no need to mount"
fi

exists=$(unzip -lq /mnt/factory/update.zip | grep recovery.img)
if [ "$exists" = "" ]; then
        echo "update.zip or recovery.img not found. aborting"
        exit 1
fi


recovery_part=$(grep "recovery\b" /proc/mtd | sed -e s/:.*//)
if [ "$recovery_part" = "" ]; then
        echo "cannot locate recovery partition"
        exit 1
fi
recovery_device=/dev/mtd/$recovery_part
if [ ! -c "$recovery_device" ]; then
        echo "cannot find /dev/mtd/$recovery_device"
        exit 1
fi

set -e

mountpoint /mnt/factory
unzip  /mnt/factory/update.zip recovery.img -d /mnt/scratch
umount /mnt/factory
flash_erase $recovery_device 0 0
nandwrite -p $recovery_device /mnt/scratch/recovery.img
rm -f /mnt/scratch/recovery.img
sync; sync; sync

