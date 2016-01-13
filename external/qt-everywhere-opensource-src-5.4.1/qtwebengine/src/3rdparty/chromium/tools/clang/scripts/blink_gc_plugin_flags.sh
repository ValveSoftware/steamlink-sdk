#!/usr/bin/env bash
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script returns the flags that should be passed to clang.

THIS_ABS_DIR=$(cd $(dirname $0) && echo $PWD)
CLANG_LIB_PATH=$THIS_ABS_DIR/../../../third_party/llvm-build/Release+Asserts/lib

if uname -s | grep -q Darwin; then
  LIBSUFFIX=dylib
else
  LIBSUFFIX=so
fi

LIBNAME=\
$(grep LIBRARYNAME "$THIS_ABS_DIR"/../blink_gc_plugin/Makefile \
    | cut -d ' ' -f 3)

FLAGS=""
for arg in "$@"; do
  if [[ "$arg" = "enable-oilpan=1" ]]; then
    FLAGS="$FLAGS -Xclang -plugin-arg-blink-gc-plugin -Xclang enable-oilpan"
  elif [[ "$arg" = "dump-graph=1" ]]; then
    FLAGS="$FLAGS -Xclang -plugin-arg-blink-gc-plugin -Xclang dump-graph"
  fi
done

echo -Xclang -load -Xclang $CLANG_LIB_PATH/lib$LIBNAME.$LIBSUFFIX \
  -Xclang -add-plugin -Xclang blink-gc-plugin $FLAGS
