// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_ANDROID_SURFACE_TEXTURE_PEER_H_
#define GPU_IPC_COMMON_ANDROID_SURFACE_TEXTURE_PEER_H_

#include "base/macros.h"
#include "base/process/process.h"
#include "gpu/gpu_export.h"
#include "ui/gl/android/surface_texture.h"

namespace gpu {

class GPU_EXPORT SurfaceTexturePeer {
 public:
  static SurfaceTexturePeer* GetInstance();

  static void InitInstance(SurfaceTexturePeer* instance);

  // Establish the producer end for the given surface texture in another
  // process.
  virtual void EstablishSurfaceTexturePeer(
      base::ProcessHandle pid,
      scoped_refptr<gl::SurfaceTexture> surface_texture,
      int primary_id,
      int secondary_id) = 0;

 protected:
  SurfaceTexturePeer();
  virtual ~SurfaceTexturePeer();

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceTexturePeer);
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_ANDROID_SURFACE_TEXTURE_PEER_H_
