// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_SURFACE_LOOKUP_H_
#define CONTENT_COMMON_GPU_GPU_SURFACE_LOOKUP_H_

#include "ui/gfx/native_widget_types.h"

namespace content {

// This class provides an interface to look up window surface handles
// that cannot be sent through the IPC channel.
class GpuSurfaceLookup {
 public:
  GpuSurfaceLookup() { }
  virtual ~GpuSurfaceLookup() { }

  static GpuSurfaceLookup* GetInstance();
  static void InitInstance(GpuSurfaceLookup* lookup);

  virtual gfx::AcceleratedWidget AcquireNativeWidget(int surface_id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuSurfaceLookup);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_SURFACE_LOOKUP_H_
