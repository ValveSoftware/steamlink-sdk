// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_IOSURFACE_MAC_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_IOSURFACE_MAC_H_

#include "content/common/gpu/image_transport_surface_fbo_mac.h"
#include "ui/gl/gl_bindings.h"

// Note that this must be included after gl_bindings.h to avoid conflicts.
#include <OpenGL/CGLIOSurface.h>

namespace content {

// Allocate IOSurface-backed storage for an FBO image transport surface.
class IOSurfaceStorageProvider
    : public ImageTransportSurfaceFBO::StorageProvider {
 public:
  IOSurfaceStorageProvider();
  virtual ~IOSurfaceStorageProvider();

  // ImageTransportSurfaceFBO::StorageProvider implementation:
  virtual gfx::Size GetRoundedSize(gfx::Size size) OVERRIDE;
  virtual bool AllocateColorBufferStorage(
      CGLContextObj context, gfx::Size size) OVERRIDE;
  virtual void FreeColorBufferStorage() OVERRIDE;
  virtual uint64 GetSurfaceHandle() const OVERRIDE;

 private:
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;

  // The id of |io_surface_| or 0 if that's NULL.
  IOSurfaceID io_surface_handle_;

  DISALLOW_COPY_AND_ASSIGN(IOSurfaceStorageProvider);
};

}  // namespace content

#endif  //  CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_MAC_H_
