#!/bin/bash

source ../../setenv_external.sh
unset CC CPP CXX AS LDFLAGS STRIP

#rm -rf build
mkdir -p build
pushd build

#../configure -release -force-debug-info -opensource -confirm-license -qt-harfbuzz -qt-libpng -no-feature-bearermanagement -no-linuxfb -no-alsa -no-pulseaudio -no-glib -device marvell -device-option CROSS_COMPILE=armv7a-cros-linux-gnueabi- -sysroot "${MARVELL_ROOTFS}" -hostprefix "${PWD}/host" -nomake examples -nomake tests || exit 1

# Need to bootstrap ninja for the qtwebengine build
mkdir -p qtwebengine/src/3rdparty/ninja
pushd qtwebengine/src/3rdparty/ninja
#../../../../../qtwebengine/src/3rdparty/ninja/configure.py --bootstrap || exit 1
popd

steamlink_make || exit 1
steamlink_make_install
