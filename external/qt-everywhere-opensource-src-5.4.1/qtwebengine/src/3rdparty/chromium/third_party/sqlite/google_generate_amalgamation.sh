#!/bin/bash
#
# Copyright 2011 Google Inc. All Rights Reserved.
# Author: shess@chromium.org (Scott Hess)
# TODO(shess): This notice needs updating.

cd src

mkdir bld
cd bld
../configure
FILES="sqlite3.h sqlite3.c"
OPTS=""
# These options should match those in sqlite.gyp.
OPTS="$OPTS -DSQLITE_OMIT_LOAD_EXTENSION=1"
make "OPTS=$OPTS" $FILES
cp -f $FILES ../../amalgamation

cd ..
rm -rf bld

# TODO(shess) I can't find config.h, which exists in the original
# third_party/sqlite/ directory.  I also haven't found a client of it,
# yet, so maybe it's not a file we need.
