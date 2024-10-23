_TOP=$TOP
TOP=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)

if [ "$MARVELL_SDK_PATH" = "" ]; then
	export MARVELL_SDK_PATH="$TOP"
fi
if [ "$MARVELL_ROOTFS" = "" ]; then
	export MARVELL_ROOTFS=$TOP/rootfs
fi

# Sanity check
if [ ! -d "$MARVELL_ROOTFS/etc" ]; then
	echo "$MARVELL_ROOTFS is not the Steam Link system image!"
	return 1
fi

# Set up CMake environment
export CMAKE_TOOLCHAIN_FILE=$TOP/toolchain/steamlink-toolchain.cmake

# Set up Qt environment
QT_VERSION=5.14.1
QT_HOST_PREFIX=$MARVELL_ROOTFS/usr/local/Qt-$QT_VERSION
QT_HOST_BINS=$QT_HOST_PREFIX/bin
cat <<__EOF__ >$QT_HOST_BINS/qt.conf
[Paths]
Sysroot = $MARVELL_ROOTFS
Prefix = /usr/local/Qt-$QT_VERSION
HostPrefix = $QT_HOST_PREFIX
__EOF__
export PATH=$QT_HOST_BINS:$PATH

TOOLCHAIN_PATH=$TOP/toolchain/bin
export PATH=$TOOLCHAIN_PATH:$TOP/bin:$PATH

# Fix libtool library path
find "$TOP/toolchain" -name '*.la' | \
while read file; do
	sed -i -e "s,libdir='.*',libdir='$(dirname $file)'," $file
done
export LD_LIBRARY_PATH="$TOP/toolchain/lib"

export CROSS=armv7a-cros-linux-gnueabi-
export CROSS_COMPILE=${CROSS}
export AS=${CROSS}as
export LD=${CROSS}ld
MARVELL_RPATH_LINK_OPTIONS="-Wl,-rpath-link,$MARVELL_ROOTFS/lib -Wl,-rpath-link,$MARVELL_ROOTFS/usr/lib -Wl,-rpath-link,$MARVELL_ROOTFS/usr/lib/arm-linux-gnueabihf"
export CC="${CROSS}gcc --sysroot=$MARVELL_ROOTFS $MARVELL_RPATH_LINK_OPTIONS"
export CXX="${CROSS}g++ --sysroot=$MARVELL_ROOTFS $MARVELL_RPATH_LINK_OPTIONS"
export CPP="${CROSS}cpp --sysroot=$MARVELL_ROOTFS"
export STRIP=${CROSS}strip
export LC_ALL=C	# Toolchain crashes with some locales

path_to_executable=$(which $(basename ${CROSS}gcc))
if [ ! -n "${path_to_executable}" ]; then
	echo "warning: $(basename ${CROSS}gcc) was not found in your PATH"
fi
unset path_to_executable

export SOC_BUILD=armv7a-cros-linux-gnueabi

export PKG_CONFIG_LIBDIR="$MARVELL_ROOTFS/usr/lib/pkgconfig:$MARVELL_ROOTFS/usr/lib/arm-linux-gnueabihf/pkgconfig:$MARVELL_ROOTFS/usr/local/Qt-$QT_VERSION/lib/pkgconfig"
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
	make $MAKE_J "$@"
}

steamlink_make_install() {
	DESTDIR=$MARVELL_ROOTFS make install "$@"
}

steamlink_make_uninstall() {
	DESTDIR=$MARVELL_ROOTFS make uninstall "$@"
}

TOP=$_TOP
