#!/bin/sh

# Install these packages to build the toolchain
#sudo apt install texinfo libgmp-dev libmpfr-dev libmpc-dev

TOP=$(cd `dirname "$0"` && pwd)
DEST="$1"
ROOTFS=$2
BINUTILS=../binutils-2.34

if [ "$DEST" = "" -o "$ROOTFS" = "" -o ! -d "$ROOTFS/etc" ]; then
    echo "Usage: $0 [dest] [rootfs]" >&2
    exit 1
fi

if [ "$MARVELL_ROOTFS" != "" ]; then
    echo "Please run this script from a clean native build environment"
    exit 1
fi

$BINUTILS/build_steamlink.sh "$DEST" || exit $?

mkdir -p ../build-gcc
cd ../build-gcc
$TOP/configure --prefix="$DEST" --with-sysroot="$ROOTFS" --host=x86_64-pc-linux-gnu --target=armv7a-cros-linux-gnueabi --enable-languages=c,c++ --disable-multilib --disable-lto --with-arch=armv7-a --with-fpu=neon --with-float=hard || exit $?
make -j10 all-gcc || exit $?
make install-gcc || exit $?
make -j10 all-target-libgcc || exit $?
make install-target-libgcc || exit $?
make -j10 || exit $?
make install || exit $?

find "$DEST" -type f | while read file
do if file "$file" | fgrep ELF | fgrep -v ARM >/dev/null; then
       strip "$file"
   fi
done

echo ""
echo "Toolchain is installed in $DEST"
