// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/android/surface_texture_manager.h"

#include "base/logging.h"

namespace gpu {
namespace {

SurfaceTextureManager* g_instance = nullptr;

}  // namespace

// static
SurfaceTextureManager* SurfaceTextureManager::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void SurfaceTextureManager::SetInstance(SurfaceTextureManager* instance) {
  DCHECK(!g_instance || !instance);
  g_instance = instance;
}

}  // namespace gpu
