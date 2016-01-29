absolute_path=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)

export MARVELL_SDK_PATH=$absolute_path
export MARVELL_ROOTFS=$MARVELL_SDK_PATH/rootfs

# Sanity check
if [ ! -d $MARVELL_ROOTFS ]; then
	echo "$MARVELL_ROOTFS does not exist!"
	return 1
fi

# Set up Qt environment
QT_HOST_PREFIX=$MARVELL_SDK_PATH/external/qt-everywhere-opensource-src-5.4.1/build/host
QT_HOST_BINS=$QT_HOST_PREFIX/bin
cat <<__EOF__ >$QT_HOST_BINS/qt.conf
[Paths]
Sysroot = $MARVELL_ROOTFS
Prefix = /usr/local/Qt-5.4.1
HostPrefix = $QT_HOST_PREFIX
__EOF__
export PATH=$QT_HOST_BINS:$PATH

TOOLCHAIN_PATH=$MARVELL_SDK_PATH/toolchain/bin
export PATH=$TOOLCHAIN_PATH:$MARVELL_SDK_PATH/bin:$PATH

export CROSS=armv7a-cros-linux-gnueabi-
export CROSS_COMPILE=${CROSS}
export AS=${CROSS}as
export CC="${CROSS}gcc --sysroot=$MARVELL_ROOTFS -marm -mfloat-abi=hard"
export CPP="${CROSS}cpp --sysroot=$MARVELL_ROOTFS -marm -mfloat-abi=hard"
export CXX="${CROSS}g++ --sysroot=$MARVELL_ROOTFS -marm -mfloat-abi=hard"
export STRIP=${CROSS}strip
export LC_ALL=C	# Toolchain crashes with some locales

export LDFLAGS="-static-libgcc -static-libstdc++"

path_to_executable=$(which $(basename ${CROSS}gcc))
if [ ! -n "${path_to_executable}" ]; then
	echo "warning: ${path_to_executable} was not found in your PATH"
fi
unset path_to_executable

export SOC_BUILD=armv7a-cros-linux-gnueabi

export PKG_CONFIG_PATH="$MARVELL_ROOTFS/usr/lib/pkgconfig:$MARVELL_ROOTFS/usr/local/Qt-5.4.1/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR=$MARVELL_ROOTFS

export STEAMLINK_CONFIGURE_OPTS="--host=$SOC_BUILD \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--with-sysroot=$MARVELL_ROOTFS"

steamlink_make_clean() {
	make clean
}

steamlink_make() {
	make $MAKE_J
}

steamlink_make_install() {
	DESTDIR=$MARVELL_ROOTFS make install
}

steamlink_make_uninstall() {
	DESTDIR=$MARVELL_ROOTFS make uninstall
}
