// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/surface_texture_peer.h"

#include "base/logging.h"

namespace content {

namespace {
SurfaceTexturePeer* g_instance_ = NULL;
}  // namespace

SurfaceTexturePeer::SurfaceTexturePeer() {
}

SurfaceTexturePeer::~SurfaceTexturePeer() {
}

// static
SurfaceTexturePeer* SurfaceTexturePeer::GetInstance() {
  DCHECK(g_instance_);
  return g_instance_;
}

// static
void SurfaceTexturePeer::InitInstance(SurfaceTexturePeer* instance) {
  DCHECK(!g_instance_);
  g_instance_ = instance;
}

}  // namespace content
