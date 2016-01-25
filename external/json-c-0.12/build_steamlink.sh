#!/bin/bash

source ../../setenv_external.sh

export ac_cv_func_malloc_0_nonnull=yes
export ac_cv_func_realloc_0_nonnull=yes
./autogen.sh
./configure $STEAMLINK_CONFIGURE_OPTS

steamlink_make_clean
steamlink_make
steamlink_make_install
