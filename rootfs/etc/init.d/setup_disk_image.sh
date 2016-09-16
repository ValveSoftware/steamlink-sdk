#!/bin/sh

if [ $# -ne 4 ]; then
    echo "Usage: $0 <disk image> <size> <loop device> <mountpoint>"
    exit 1
fi
DISK_IMAGE_FILE=$1
DISK_IMAGE_FILE_SZ=$2
DISK_IMAGE_LOOP_DEVICE=$3
DISK_IMAGE_MOUNTPOINT=$4

create_loop_node() {
    if [ ! -b "$DISK_IMAGE_LOOP_DEVICE" ]; then
        node=$(echo "$DISK_IMAGE_LOOP_DEVICE" | sed 's/.*\([0-9]\+\)/\1/')
        if [ -z "$node" ]; then
            echo "Couldn't determine loop node from $DISK_IMAGE_LOOP_DEVICE"
            exit 1
        fi
        mknod "$DISK_IMAGE_LOOP_DEVICE" b 7 $node
    fi
}

create_loop_file_if_not_exist() {
    if [ ! -f "$DISK_IMAGE_FILE" ]; then
        # Create a sparse file for image
        dd if=/dev/zero of="$DISK_IMAGE_FILE" bs=1 count=0 seek=$DISK_IMAGE_FILE_SZ
        ret=$?
        if [ $ret -ne 0 ]; then
            echo "dd if=/dev/zero of=$DISK_IMAGE_FILE bs=1 count=0 seek=$DISK_IMAGE_FILE_SZ failed: $ret"
        fi
    fi
}

setup_loopback_device() {
    # Attach file to loopback device
    losetup "$DISK_IMAGE_LOOP_DEVICE" "$DISK_IMAGE_FILE"
    ret=$?
    if [ $ret -ne 0 ]; then
        echo "losetup $DISK_IMAGE_LOOP_DEVICE $DISK_IMAGE_FILE failed: $ret"
    fi
}

format_loopback_device() {
    mkfs.ext4 "$DISK_IMAGE_LOOP_DEVICE"
    ret=$?
    if [ $ret -ne 0 ]; then
        echo "mkfs.ext2 $DISK_IMAGE_LOOP_DEVICE failed: $ret"
    fi
}

mount_or_format_loopback_device() {
    # Create mountpoint
    mkdir -p "$DISK_IMAGE_MOUNTPOINT"
    # Try to mount, if it fails, reformat
    mount "$DISK_IMAGE_LOOP_DEVICE" "$DISK_IMAGE_MOUNTPOINT"
    ret=$?
    if [ $ret -ne 0 ]; then
        # Mounting failed, reformat
        echo "mount $DISK_IMAGE_LOOP_DEVICE $DISK_IMAGE_MOUNTPOINT failed: $ret, reformatting"
        format_loopback_device
        # Try again
        mount "$DISK_IMAGE_LOOP_DEVICE" "$DISK_IMAGE_MOUNTPOINT"
        ret=$?
        if [ $ret -ne 0 ]; then
            echo "mount $DISK_IMAGE_LOOP_DEVICE $DISK_IMAGE_MOUNTPOINT failed: $ret, giving up"
        fi
    fi
}

check_for_status() {
    mount_status=$(grep "$DISK_IMAGE_LOOP_DEVICE" /proc/mounts)
    if [ -n "$mount_status" ]; then
        return 0
    else
        return 1
    fi
}

unmount() {
    if check_for_status; then
        umount "$DISK_IMAGE_LOOP_DEVICE"
    fi
}

loop_start() {
    if check_for_status; then
        echo "Already mounted!"
        return 0
    fi
    create_loop_node
    create_loop_file_if_not_exist
    setup_loopback_device
    mount_or_format_loopback_device
    check_for_status
}

loop_stop() {
    unmount
    if check_for_status; then
        return 1
    else
        return 0
    fi
}

loop_delete() {
    unmount
    rm -f "$DISK_IMAGE_FILE"
}

loop_start
