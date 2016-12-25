#!/bin/bash
#

TOP="${PWD}"
SRC="${TOP}/caveexpress-src"

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
source "${TOP}/../../setenv.sh"

pushd "${SRC}"
./contrib/scripts/steamlink.sh
popd

#
# Install it
#
mkdir -p "${TOP}/steamlink/apps"
cp -av cp-build-steamlink/steamlink/* "${TOP}/steamlink/apps" || exit 1

#
# All done!
#
echo "Build complete!"
echo
echo "Put the steamlink folder onto a USB drive, insert it into your Steam Link, and cycle the power to install."
