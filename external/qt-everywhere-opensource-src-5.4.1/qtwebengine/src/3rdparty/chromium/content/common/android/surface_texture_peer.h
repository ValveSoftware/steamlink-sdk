// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_PEER_H_
#define CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_PEER_H_

#include "base/process/process.h"
#include "ui/gl/android/surface_texture.h"

namespace content {

class SurfaceTexturePeer {
 public:
  static SurfaceTexturePeer* GetInstance();

  static void InitInstance(SurfaceTexturePeer* instance);

  // Establish the producer end for the given surface texture in another
  // process.
  virtual void EstablishSurfaceTexturePeer(
      base::ProcessHandle pid,
      scoped_refptr<gfx::SurfaceTexture> surface_texture,
      int primary_id,
      int secondary_id) = 0;

 protected:
  SurfaceTexturePeer();
  virtual ~SurfaceTexturePeer();

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceTexturePeer);
};

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_PEER_H_
