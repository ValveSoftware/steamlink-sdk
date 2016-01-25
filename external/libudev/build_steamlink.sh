#!/bin/bash

source ../../setenv_external.sh

#sed -e 's/#define HAVE_DECL_SETNS.*/#define HAVE_DECL_SETNS 0/' \
#	-e 's/#define HAVE_DECL_NAME_TO_HANDLE_AT.*/#define HAVE_DECL_NAME_TO_HANDLE_AT 0/' \
#	<config.h.in >config.h
cp config.h.in config.h

steamlink_make_clean
steamlink_make
steamlink_make_install
