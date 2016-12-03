#!/bin/bash

source ../../setenv_external.sh
unset CC CPP CXX AS LDFLAGS

#rm -rf build
mkdir -p build
pushd build
../configure -release -force-debug-info -opensource -confirm-license -qt-harfbuzz -qt-libpng -no-kms -no-linuxfb -no-xcb -device marvell -device-option CROSS_COMPILE=armv7a-cros-linux-gnueabi- -sysroot "${MARVELL_ROOTFS}" -hostprefix "${PWD}/host" -nomake examples -nomake tests -skip qtlocation -skip qtquick1 -skip qtquickcontrols -skip qtscript -skip qtsensors -skip qtserialport -skip qtsvg -skip qtwebchannel -skip qtwebengine -skip qtwebkit -skip qtwebsockets -skip qtxmlpatterns -v

steamlink_make
steamlink_make_install
