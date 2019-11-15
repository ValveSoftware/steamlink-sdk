#!/bin/bash

source ../../setenv_external.sh

pushd external/jpeg-9b || exit 1

./configure $STEAMLINK_CONFIGURE_OPTS

steamlink_make_clean
steamlink_make
steamlink_make_install

popd

./configure $STEAMLINK_CONFIGURE_OPTS

steamlink_make_clean
steamlink_make
steamlink_make_install

