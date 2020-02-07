#!/bin/sh

TOP=$(cd `dirname "$0"` && pwd)
DEST=$1

if [ "$DEST" = "" ]; then
    echo "Please run this as part of the toolchain build in the gcc directory" >&2
    exit 1
fi

mkdir -p ../build-binutils
cd ../build-binutils
$TOP/configure -prefix="$DEST" --target=armv7a-cros-linux-gnueabi --disable-multilib --with-float=hard || exit $?
make -j10 || exit $?
make install || exit $?
