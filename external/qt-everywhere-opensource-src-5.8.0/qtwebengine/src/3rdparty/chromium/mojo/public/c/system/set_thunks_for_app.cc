// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/thunks.h"

extern "C" {

#if defined(WIN32)
#define THUNKS_EXPORT __declspec(dllexport)
#else
#define THUNKS_EXPORT __attribute__((visibility("default")))
#endif

THUNKS_EXPORT size_t MojoSetSystemThunks(
    const MojoSystemThunks* system_thunks) {
  return MojoEmbedderSetSystemThunks(system_thunks);
}

}  // extern "C"
