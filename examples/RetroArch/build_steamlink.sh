#!/bin/bash
#

TOP=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
if [ "${MARVELL_SDK_PATH}" = "" ]; then
	MARVELL_SDK_PATH="$(cd "${TOP}/../.." && pwd)"
fi
if [ "${MARVELL_ROOTFS}" = "" ]; then
	source "${MARVELL_SDK_PATH}/setenv.sh" || exit 1
	source "setenv_extra.sh" || exit 1
fi
cd "${TOP}"


RETROARCH_SRC="${PWD}/retroarch-src"

#
# Download the source
#
if [ ! -d "${RETROARCH_SRC}" ]; then
	git clone https://github.com/libretro/RetroArch.git "${RETROARCH_SRC}"
fi

#Generate pkg-config files for gles2 and egl
cat > ${MARVELL_ROOTFS}/usr/lib/pkgconfig/egl.pc <<__EOF__
prefix=/usr
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: EGL
Description: Manual EGL package
Version: 200
Libs: -L${libdir} -lEGL -lGLESv2
Cflags: -I${includedir}

__EOF__

cat > ${MARVELL_ROOTFS}/usr/lib/pkgconfig/glesv2.pc <<__EOF__
prefix=/usr
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: GLESv2
Description: Manual GL ES 2 package
Version: 2.0
Libs: -L${libdir} -lGLESv2
Cflags: -I${includedir}

__EOF__

pushd "${RETROARCH_SRC}"

./configure --host=$SOC_BUILD --disable-sdl --enable-sdl2 --disable-libusb --disable-udev --disable-threads --disable-ffmpeg --disable-ssa --enable-networking --disable-d3d9 --enable-opengl --enable-opengles --disable-x11 --disable-wayland --enable-egl --disable-kms --enable-zlib --enable-alsa --enable-pulse --enable-freetype --disable-xvideo --enable-neon --enable-floathard --disable-7zip --disable-libxml2 --disable-shaderpipeline --disable-vg --disable-cg

make $MAKE_J || exit 2

export DESTDIR="${PWD}/steamlink/apps/retroarch"


# Copy the files to the app directory
mkdir -p "${DESTDIR}"
mkdir -p "${DESTDIR}/roms"
mkdir -p "${DESTDIR}/extra"

cp -v retroarch ../retroarch-run.sh ../retroarch-icon.png ../retroarch_steamlink.cfg "${DESTDIR}"

# You can add some roms for testing
if [ -d "roms" ]; then
	cp -v -r roms/* "${DESTDIR}/roms"
fi
# Put the compiled cores or other files in a /extra folder to add them
if [ -d "extra" ]; then
	cp -v -r extra/* "${DESTDIR}/extra"
fi

# Create the table of contents and icon
cat >"${DESTDIR}/toc.txt" <<__EOF__
name=Retroarch
icon=retroarch-icon.png
run=retroarch-run.sh
__EOF__


# Pack it up
name=$(basename ${DESTDIR})
pushd "$(dirname ${DESTDIR})"
tar zcvf $name.tgz $name || exit 3
rm -rf $name
popd

# All done!
echo "Build complete!"
echo
echo "Put the steamlink folder onto a USB drive, insert it into your Steam Link, and cycle the power to install."
