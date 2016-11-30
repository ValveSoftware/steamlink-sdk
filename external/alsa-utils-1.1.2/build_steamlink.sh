#!/bin/bash

source ../../setenv_external.sh

export LIBS="-ltinfo"

./configure  $STEAMLINK_CONFIGURE_OPTS \
  --disable-xmlto \
  --disable-nls \


steamlink_make_clean
steamlink_make

