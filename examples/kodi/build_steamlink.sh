#!/bin/bash
#

TOP="${PWD}"
SRC="${TOP}/kodi-src"
NCPU=`grep -c processor /proc/cpuinfo`

#
# Download the source to Kodi
#
if [ ! -d "${SRC}" ]; then
	git clone -b "Krypton" https://github.com/xbmc/xbmc.git "${SRC}" || exit 1
	rm -f "${TOP}/.patch-applied"
fi

#
# Apply any patches and bootstrap it
#
if [ "${TOP}/kodi.patch" -nt "${TOP}/.patch-applied" ]; then
	pushd "${SRC}"
	git clean -fxd
	git checkout .
	git am <"${TOP}/kodi.patch" || exit 1
	popd
	touch "${TOP}/.patch-applied"
fi

if [ ! -f "${SRC}/configure" ]; then
	pushd "${SRC}"
	./bootstrap || exit 1
	popd
fi

if [ ! -f "${SRC}/tools/depends/configure" ]; then
	pushd "${SRC}/tools/depends"
	./bootstrap || exit 1
	popd
fi

#
# Configure build dependencies
#
MARVELL_SDK_PATH=$(cd ../.. && pwd)
MARVELL_ROOTFS="${MARVELL_SDK_PATH}/rootfs"
SOC_BUILD=armv7a-cros-linux-gnueabi

DEPS_INSTALL_PATH="${MARVELL_SDK_PATH}/kodi-deps/${SOC_BUILD}"
DEPS_CONFIG_SITE="${DEPS_INSTALL_PATH}/share/config.site"
DEPS_TOOLCHAIN_CMAKE="${DEPS_INSTALL_PATH}/share/Toolchain.cmake"

if [ ! -f "${SRC}/tools/depends/Makefile.include" ]; then
	# Run this in a subshell so we don't set CC and so forth yet
	(
		source "${TOP}/../../setenv.sh"

		pushd "${SRC}/tools/depends"
		./configure --with-toolchain="${MARVELL_SDK_PATH}/toolchain" --prefix="${MARVELL_SDK_PATH}/kodi-deps" --host=${SOC_BUILD} || exit 2

		cat >"${DEPS_CONFIG_SITE}.new" << __EOF__
MARVELL_SDK_PATH="${MARVELL_SDK_PATH}"
MARVELL_ROOTFS="${MARVELL_ROOTFS}"

__EOF__
		sed \
			-e "s,^CC=.*,CC=\"${MARVELL_SDK_PATH}/toolchain/bin/${CC}\"," \
			-e "s,^CXX=.*,CXX=\"${MARVELL_SDK_PATH}/toolchain/bin/${CXX}\"," \
			-e "s,^CPP=.*,CPP=\"\${CC} -E\"," \
			"${DEPS_CONFIG_SITE}" \
			>>"${DEPS_CONFIG_SITE}.new"
		mv "${DEPS_CONFIG_SITE}.new" "${DEPS_CONFIG_SITE}"

		sed -i \
			-e "s,^SET(CMAKE_C_FLAGS .*,SET(CMAKE_C_FLAGS \" --sysroot=${MARVELL_ROOTFS} -marm -mfloat-abi=hard -isystem ${DEPS_INSTALL_PATH}/include\")," \
			-e "s,^SET(CMAKE_CXX_FLAGS .*,SET(CMAKE_CXX_FLAGS \" --sysroot=${MARVELL_ROOTFS} -marm -mfloat-abi=hard -g -O2 -std=gnu++11 -isystem ${DEPS_INSTALL_PATH}/include\")," \
			"${DEPS_TOOLCHAIN_CMAKE}"

		sed -i \
			-e "s,^CPU=.*,CPU=armv7-a," \
			-e "s,^OS=.*,OS=linux," \
			-e "s,^CC=.*,CC=${MARVELL_SDK_PATH}/toolchain/bin/${CC}," \
			-e "s,^CXX=.*,CXX=${MARVELL_SDK_PATH}/toolchain/bin/${CXX}," \
			-e "s,^CPP=.*,CPP=\$(CC) -E," \
			Makefile.include
	)
fi

# GMP from the Steam Link SDK conflicts with Kodi
if [ -f "${MARVELL_ROOTFS}/usr/include/gmp.h" ]; then
	# Run this in a subshell so we don't set CC and so forth yet
	(
		source "${TOP}/../../setenv.sh"
		cd "${MARVELL_SDK_PATH}/external/gmp-6.0.0"
		if [ ! -f Makefile ]; then
			./configure $STEAMLINK_CONFIGURE_OPTS
		fi
		steamlink_make_uninstall
	)
fi

#
# Build native dependencies
#
pushd "${SRC}/tools/depends"
make -C native || exit 2
popd

#
# Build target dependencies
#
pushd "${SRC}/tools/depends"

# We'll use some libraries from the Steam Link SDK
function satisfy_dependency
{
	# Build the dependency but don't actually install it
	path="${SRC}/tools/depends/target/$1"
	make -C "${path}" $2
	touch "${path}/.installed-${SOC_BUILD}"
}
satisfy_dependency alsa-lib "${SOC_BUILD}/src/.libs/libasound.so"
satisfy_dependency libsdl2 "${SOC_BUILD}/build/.libs/libSDL2.a"
satisfy_dependency libogg "${SOC_BUILD}/src/.libs/libogg.a"
satisfy_dependency libvorbis "${SOC_BUILD}/lib/.libs/libvorbis.a"

# Fix libgpg-error
pushd target/libgpg-error
make ${SOC_BUILD}
if ! grep "host_triplet = arm-unknown-linux-gnueabi" "${SOC_BUILD}/src/Makefile" >/dev/null; then
	sed -i "s,host_triplet = .*,host_triplet = arm-unknown-linux-gnueabi," "${SOC_BUILD}/src/Makefile"
fi
popd

# make everything
make -C target || exit 3
make -C target/samba || exit 3

# Build the shared library version of OpenSSL
# We need to do this after make target because something in that process
# sometimes removes the shared library links
if [ ! -L "${DEPS_INSTALL_PATH}/lib/libssl.so" ]; then
	pushd target/openssl
	make || exit 3
	cp -av ${SOC_BUILD}/lib*.so* "${DEPS_INSTALL_PATH}/lib"
	popd
	for dependency in curl librtmp; do
		rm -rf target/${dependency}/${SOC_BUILD}
		make -C target/${dependency}
	done
fi

# Build binary add-ons
make -C target/binary-addons PREFIX="${TOP}/steamlink/apps/kodi" -j20 || exit 3

# All done!

popd

#
# Finally build Kodi
#
source "${TOP}/../../setenv.sh"
source "${DEPS_CONFIG_SITE}"

export CC="$CC -DEGL_API_FB"
export CXX="$CXX -DEGL_API_FB"
export CFLAGS CXXFLAGS CPPFLAGS LDFLAGS
export LD AR AS NM STRIP RANLIB OBJDUMP
export PYTHON_VERSION PYTHON_CPPFLAGS PYTHON_LDFLAGS PYTHON_SITE_PKG PYTHON_NOVERSIONCHECK NATIVE_ROOT
export UDEV_CFLAGS="-I${DEPS_INSTALL_PATH}/include"
export UDEV_LIBS=-ludev
export CEC_CFLAGS="-I${DEPS_INSTALL_PATH}/include"
export CEC_LIBS=-lcec
export LIBXML_CFLAGS="-I${DEPS_INSTALL_PATH}/include/libxml2"
export PULSE_CFLAGS="-I${MARVELL_ROOTFS}/usr/include"
export PULSE_LIBS="-lpulse -L${MARVELL_ROOTFS}/usr/lib/pulseaudio -lpulsecommon-8.0"
export PKG_CONFIG_SYSROOT_DIR="${MARVELL_ROOTFS}"
pushd "${SRC}"
./configure $STEAMLINK_CONFIGURE_OPTS --prefix=/home/apps/kodi --disable-x11 || exit 4

make clean
make -j${NCPU} || exit 5

export DESTDIR="${TOP}/steamlink/apps/kodi"
make install
for dir in "${DESTDIR}/home/apps/kodi"/*; do
    cp -av "$dir" "${DESTDIR}"
done

# Sanity check
if [ "${DESTDIR}/home" == "/home" ]; then
    echo "Aborting!"
    exit 6
fi
rm -rf "${DESTDIR}/home"

cp -a ${DEPS_INSTALL_PATH}/lib/python2.7 ${DESTDIR}/lib/

for i in \
	libass.so.5 \
	libbluray.so.1 \
	libcec.so.4.0.0 \
	libcrypto.so.1.0.0 \
	libcurl.so.4 \
	libnfs.so.8 \
	libplist.so.1 \
	libshairplay.so.0 \
	libsmbclient.so.0 \
	libssl.so.1.0.0 \

do
    library="${DEPS_INSTALL_PATH}/lib/$i"
    if [ -f "${library}" ]; then
        target="${DESTDIR}/lib/$i"
        cp -v "${library}" "${target}"
        chmod 755 "${target}"
    else
        echo "Warning: Couldn't find $i"
    fi
done

# Strip the binaries
find ${DESTDIR} -type f | while read file; do
    if file ${file} | grep ELF >/dev/null; then
        echo "Stripping $(basename ${file})"
        armv7a-cros-linux-gnueabi-strip ${file}
    fi
done

# Add the toc
cat >"${DESTDIR}/toc.txt" <<__EOF__
name=Kodi
icon=icon.png
run=kodi.sh
__EOF__

# Add the start script
cat >"${DESTDIR}/kodi.sh" <<__EOF__
#!/bin/sh
#
# Start Kodi with the correct environment on the Steam Link

export LD_LIBRARY_PATH="\${LD_LIBRARY_PATH}:/usr/lib/pulseaudio:\${PWD}/lib"
export KODI_HOME="\${PWD}/share/kodi"
export KODI_HOME_BIN="\${PWD}/lib/kodi"
export PYTHONHOME="\${PWD}"
exec ./bin/kodi
__EOF__
chmod 755 "${DESTDIR}/kodi.sh"

# Add the icon
base64 -d >"${DESTDIR}/icon.png" <<__EOF__
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
lUsF4P8BXBKtqr0J9mEAAAAASUVORK5CYII=
__EOF__

# Pack it up
name=$(basename ${DESTDIR})
pushd "$(dirname ${DESTDIR})"
tar zcvf $name.tgz $name
rm -rf $name
popd

# All done!
echo "Build complete!"
echo
echo "Put the steamlink folder onto a USB drive, insert it into your Steam Link, and cycle the power to install."
