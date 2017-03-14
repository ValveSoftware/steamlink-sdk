#!/bin/bash

source ../../setenv_external.sh

export LIBS="-ltinfo"
export CFLAGS="-I$MARVELL_SDK_PATH/external/ortp/include"
export BLUEZ_CFLAGS="-I$MARVELL_SDK_PATH/external/bluez-5.43/lib"
export BLUEZ_LIBS="-L$MARVELL_SDK_PATH/external/bluez-5.43/lib -lbluetooth"

autoreconf --install

./configure $STEAMLINK_CONFIGURE_OPTS \
    --disable-hcitop \
    --enable-debug \
    --enable-msbc \

steamlink_make_clean
steamlink_make 
steamlink_make_install

