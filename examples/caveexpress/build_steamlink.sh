#!/bin/bash
#

TOP=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
if [ "${MARVELL_SDK_PATH}" = "" ]; then
	MARVELL_SDK_PATH="$(cd "${TOP}/../.." && pwd)"
fi
if [ "${MARVELL_ROOTFS}" = "" ]; then
	source "${MARVELL_SDK_PATH}/setenv.sh" || exit 1
fi
BUILD="${PWD}"
SRC="${BUILD}/caveexpress-src"

#
# Download the source to caveexpress
#
if [ ! -d "${SRC}" ]; then
	git clone https://github.com/mgerhardy/caveexpress.git "${SRC}"
fi
git -C "${SRC}" checkout tags/2.4

#
# Build it
#
pushd "${SRC}"
./contrib/scripts/steamlink.sh
popd

#
# Install it
#
APPSDIR="${BUILD}/steamlink/apps"
mkdir -p "${APPSDIR}"
cp -av cp-build-steamlink/steamlink/* "${APPSDIR}" || exit 1
pushd "${APPSDIR}"
for app in caveexpress cavepacker; do
	tar zcvf $app.tgz $app || exit 3
	rm -rf $app
done
popd

#
# All done!
#
echo "Build complete!"
echo
echo "Put the steamlink folder onto a USB drive, insert it into your Steam Link, and cycle the power to install."
