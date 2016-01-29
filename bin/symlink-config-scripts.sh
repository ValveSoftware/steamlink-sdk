#!/bin/sh

for file in ../rootfs/usr/bin/*-config; do
    ln -sf $file .
    ln -sf $file armv7a-cros-linux-gnueabi-$(basename $file)
done
