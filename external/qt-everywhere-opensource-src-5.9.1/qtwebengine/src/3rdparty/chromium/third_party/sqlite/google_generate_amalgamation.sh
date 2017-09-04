#!/bin/bash
#
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cd src

mkdir bld
cd bld
../configure
FILES="sqlite3.h sqlite3.c"
OPTS=""
make "OPTS=$OPTS" $FILES
cp -f $FILES ../../amalgamation

cd ..
rm -rf bld
