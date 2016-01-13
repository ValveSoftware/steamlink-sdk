// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_LOOKUP_H_
#define CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_LOOKUP_H_

#include "ui/gfx/native_widget_types.h"

namespace content {

class SurfaceTextureLookup {
 public:
  static SurfaceTextureLookup* GetInstance();
  static void InitInstance(SurfaceTextureLookup* instance);

  virtual gfx::AcceleratedWidget AcquireNativeWidget(int primary_id,
                                                     int secondary_id) = 0;

 protected:
  virtual ~SurfaceTextureLookup() {}
};

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_LOOKUP_H_
