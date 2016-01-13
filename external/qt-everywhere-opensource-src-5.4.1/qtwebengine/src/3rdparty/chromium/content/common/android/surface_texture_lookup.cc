// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/surface_texture_lookup.h"

#include "base/logging.h"

namespace content {

namespace {
SurfaceTextureLookup* g_instance = NULL;
}  // namespace

// static
SurfaceTextureLookup* SurfaceTextureLookup::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void SurfaceTextureLookup::InitInstance(SurfaceTextureLookup* instance) {
  DCHECK(!g_instance || !instance);
  g_instance = instance;
}

}  // namespace content
