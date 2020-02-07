#!/bin/bash

source ../../setenv_external.sh

steamlink_make_clean
steamlink_make -C nss nss_build_all BUILD_OPT=1 NSPR_CONFIGURE_OPTS="$STEAMLINK_CONFIGURE_OPTS"
steamlink_make_install
