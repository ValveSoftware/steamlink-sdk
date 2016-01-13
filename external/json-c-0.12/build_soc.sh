#!/bin/bash

source ../../setenv_external.sh

export ac_cv_func_malloc_0_nonnull=yes
export ac_cv_func_realloc_0_nonnull=yes
./autogen.sh
./configure $VALVE_CONFIGURE_OPTS

valve_make_clean
valve_make
valve_make_install
