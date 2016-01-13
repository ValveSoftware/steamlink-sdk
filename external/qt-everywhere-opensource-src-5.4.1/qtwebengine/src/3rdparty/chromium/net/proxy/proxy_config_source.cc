// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config_source.h"

#include "base/basictypes.h"
#include "base/logging.h"

namespace net {

namespace {

const char* kSourceNames[] = {
  "UNKNOWN",
  "SYSTEM",
  "SYSTEM FAILED",
  "GCONF",
  "GSETTINGS",
  "KDE",
  "ENV",
  "CUSTOM",
  "TEST"
};
COMPILE_ASSERT(ARRAYSIZE_UNSAFE(kSourceNames) == NUM_PROXY_CONFIG_SOURCES,
               source_names_incorrect_size);

}  // namespace

const char* ProxyConfigSourceToString(ProxyConfigSource source) {
  DCHECK_GT(NUM_PROXY_CONFIG_SOURCES, source);
  return kSourceNames[source];
}

}  // namespace net
