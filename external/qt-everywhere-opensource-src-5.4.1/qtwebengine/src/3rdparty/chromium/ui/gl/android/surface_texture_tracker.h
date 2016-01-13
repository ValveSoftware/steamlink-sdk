// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_SURFACE_TEXTURE_TRACKER_H_
#define UI_GL_ANDROID_SURFACE_TEXTURE_TRACKER_H_

#include "base/memory/ref_counted.h"
#include "ui/gl/gl_export.h"

namespace gfx {

class SurfaceTexture;

// This interface is used to take ownership of preallocated surface textures
// with specific ids.
class GL_EXPORT SurfaceTextureTracker {
 public:
  static SurfaceTextureTracker* GetInstance();
  static void InitInstance(SurfaceTextureTracker* tracker);

  virtual scoped_refptr<SurfaceTexture> AcquireSurfaceTexture(
      int primary_id,
      int secondary_id) = 0;

 protected:
  virtual ~SurfaceTextureTracker() {}
};

}  // namespace gfx

#endif  // UI_GL_ANDROID_SURFACE_TEXTURE_TRACKER_H_
