#!/bin/bash

source ../../setenv_external.sh

./configure --with-pcap=linux $STEAMLINK_CONFIGURE_OPTS

steamlink_make_clean
steamlink_make
steamlink_make_install

