// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_H_
#define GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gl/gl_surface.h"

namespace gpu {
class GpuChannelManager;
class GpuCommandBufferStub;

// The GPU process is agnostic as to how it displays results. On some platforms
// it renders directly to window. On others it renders offscreen and transports
// the results to the browser process to display. This file provides a simple
// framework for making the offscreen path seem more like the onscreen path.

class ImageTransportSurface {
 public:
#if defined(OS_MACOSX)
  GPU_EXPORT static void SetAllowOSMesaForTesting(bool allow);
#endif

  // Creates the appropriate native surface depending on the GL implementation.
  // This will be implemented separately by each platform. On failure, a null
  // scoped_refptr should be returned.
  static scoped_refptr<gl::GLSurface> CreateNativeSurface(
      GpuChannelManager* manager,
      GpuCommandBufferStub* stub,
      SurfaceHandle surface_handle,
      gl::GLSurface::Format format);

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageTransportSurface);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_H_
