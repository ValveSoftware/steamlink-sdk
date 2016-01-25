#!/bin/bash

ICU_SOURCES=$PWD

# Build native ICU

if [ ! -d build_icu_linux ]; then
	mkdir build_icu_linux
	pushd build_icu_linux
	sh $ICU_SOURCES/source/runConfigureICU Linux --prefix=$PWD/icu_build --enable-extras=no --enable-tests=no --enable-samples=no
	make -j4
	make install
	popd
fi

# Build cross ICU

source ../../setenv_external.sh

rm -rf build_icu_marvell
mkdir build_icu_marvell
pushd build_icu_marvell
sh $ICU_SOURCES/source/configure --with-cross-build=$ICU_SOURCES/build_icu_linux --enable-extras=no --enable-tests=no --enable-samples=no $VALVE_CONFIGURE_OPTS

valve_make_clean
valve_make
valve_make_install
popd
