// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/surface_texture_tracker.h"

#include "base/logging.h"

namespace gfx {
namespace {
SurfaceTextureTracker* g_instance = NULL;
}  // namespace

// static
SurfaceTextureTracker* SurfaceTextureTracker::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void SurfaceTextureTracker::InitInstance(SurfaceTextureTracker* tracker) {
  DCHECK(!g_instance);
  g_instance = tracker;
}

}  // namespace gfx
