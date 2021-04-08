#!/bin/bash

mkdir -p build_soc
cd build_soc

LIBDIR=../../../lib
source ../../../../common_soc.sh || exit 1

if [ -z "$SOC_BUILD" ]; then
    echo "Please set the SOC_BUILD environment variable to continue"
    exit 1
fi

case $SOC_PLATFORM in
    marvell)
        if [ -z "$MARVELL_SDK_PATH" ]; then
            echo "Please source setenv_external.sh from the Marvell firmware tree"
            exit 1
        fi

        export AS=${CROSS}as
        export AR=${CROSS}ar
        export RANLIB=${CROSS}ranlib
        export DIRECTFBCONFIG="$MARVELL_SDK_PATH/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/bin/directfb-config"
        export PKG_CONFIG_PATH="$MARVELL_SDK_PATH/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/lib/pkgconfig"
        export CFLAGS="-g -O2 -Wno-multichar -DMARVELL \
-I$MARVELL_SDK_PATH/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/include"
        CONFIGURE_FLAGS="$STEAMLINK_CONFIGURE_OPTS"
        export DESTDIR=$MARVELL_ROOTFS
        ;;
esac
../configure $CONFIGURE_FLAGS

make clean && make -j4 || exit 2
make install || exit 3

if [[ "$MARVELL_SDK_PATH" != "" && "${P4CONFIG}${P4CLIENT}" != "" ]]; then
	p4 edit "$MARVELL_SDK_PATH/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/..."
	p4 revert "$MARVELL_SDK_PATH/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/bin/..."
	p4 revert -a "$MARVELL_SDK_PATH/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/..."
fi

#copy_lib build/.libs/libSDL2.a
#copy_lib build/libSDL2main.a
