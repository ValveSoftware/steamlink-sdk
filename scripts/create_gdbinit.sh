#!/bin/sh

if [ -z "$MARVELL_SDK_PATH" ]; then
	echo "Please set MARVELL_SDK_PATH"
	exit 1
fi

executable=$1
target=$2

usage() {
	echo "create_gdbinit.sh <executable> <target>"
	echo "Example:"
	echo "create_gdbbinit.sh example 10.1.211.126:8080"
}

if [ -z "$executable" ]; then
	usage
	exit 1
fi

if [ -z "$target" ]; then
	usage
	exit 1
fi

ROOT=$MARVELL_SDK_PATH/rootfs

echo "set sysroot $ROOT" > .gdbinit
echo "set auto-load safe-path $MARVELL_SDK_PATH" >> .gdbinit
echo "set debug-file-directory $MARVELL_SDK_PATH/toolchain/usr/lib/debug/usr/armv7a-cros-linux-gnueabi" >> .gdbinit
echo "file $executable" >> .gdbinit
echo "target extended-remote $target" >> .gdbinit

