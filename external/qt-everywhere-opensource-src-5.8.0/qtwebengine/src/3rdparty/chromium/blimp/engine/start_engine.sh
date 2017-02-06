#!/bin/bash

# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Luanch stunnel once and only once. This should never crash, but if it does,
# everything should die.
stunnel \
  -p /engine/data/stunnel.pem \
  -P /engine/stunnel.pid \
  -d 25466 -r 25467 -f &

# Start (and restart) the engine so long as there hasn't been an error.
# Currently, the engine can cleanly exit in the event that a connection is lost.
# In these cases, it's safe to restart the engine. However, if either stunnel or
# the engine exit with a nonzero return code, stop all execution.
while :; do
  LD_LIBRARY_PATH=/engine/ /engine/blimp_engine_app \
    --android-fonts-path=/engine/fonts \
    --blimp-client-token-path=/engine/data/client_token \
    $@ &

  # Wait for a process to exit. Bomb out if anything had an error.
  wait -n  # Returns the exited process's return code.
  retcode=$?
  if [ $retcode -ne 0 ]; then
    exit $retcode
  fi
done
