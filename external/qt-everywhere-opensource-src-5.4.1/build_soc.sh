#!/bin/bash

source ../../setenv_external.sh
unset CC CPP CXX AS LDFLAGS
#export LDFLAGS="${LDFLAGS} --sysroot=${MARVELL_SDK_PATH}/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve"

#rm -rf build
mkdir -p build
pushd build
../configure -release -force-debug-info -opensource -confirm-license -qt-harfbuzz -no-kms -no-linuxfb -no-xcb -device marvell -device-option CROSS_COMPILE=armv7a-cros-linux-gnueabi- -sysroot ${MARVELL_SDK_PATH}/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve -hostprefix `pwd`/host -nomake examples -nomake tests -skip qtlocation -skip qtquick1 -skip qtquickcontrols -skip qtscript -skip qtsensors -skip qtserialport -skip qtsvg -skip qtwebchannel -skip qtwebengine -skip qtwebkit -skip qtwebsockets -skip qtxmlpatterns -v

# Add support for keypad navigation
#echo "#define QT_KEYPAD_NAVIGATION" >>qtbase/src/corelib/global/qconfig.h

valve_make
valve_make_install
