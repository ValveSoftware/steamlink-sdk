export OS=Linux

export TOOLCHAIN_PATH=$MARVELL_SDK_PATH/toolchain/bin
export PATH=$TOOLCHAIN_PATH:$MARVELL_SDK_PATH/bin:$MARVELL_ROOTFS/usr/include/lib:$PATH

export CC="${CROSS}gcc"
export CPP="${CROSS}cpp"
export CXX="${CROSS}g++"
export CXX2="${CROSS}g++"

#PKG_CONFIG vars
export PKG_CONFIG_SYSROOT_DIR=""
export PKG_CONFIG_DIR="${MARVELL_ROOTFS}/usr/lib/pkgconfig:${MARVELL_ROOTFS}/lib/pkgconfig"
export PKG_CONFIG_LIBDIR=${MARVELL_ROOTFS}

export LDFLAGS="--sysroot=$MARVELL_ROOTFS -static-libgcc -static-libstdc++ -lEGL"
export CFLAGS="--sysroot=$MARVELL_ROOTFS -DLINUX=1 -DEGL_API_FB=1"
export INCLUDE_DIRS="-I$MARVELL_ROOTFS/usr/include -I$MARVELL_ROOTFS/usr/include/EGL -I$MARVELL_ROOTFS/usr/include/SDL2 -I${MARVELL_ROOTFS}/include/GLES2 -I${MARVELL_ROOTFS}/include"

export LIBRARY_DIRS="-L$MARVELL_ROOTFS/usr/lib -L$MARVELL_ROOTFS/lib"

export PKG_CONF_PATH=${MARVELL_SDK_PATH}/examples/RetroArch/pkg-config-custom
