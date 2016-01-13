#!/bin/bash

source ../../setenv_external.sh

./configure --with-pcap=linux $VALVE_CONFIGURE_OPTS

valve_make_clean
valve_make
valve_make_install

