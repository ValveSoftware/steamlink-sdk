// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_BUFFER_H_
#define UI_OZONE_PLATFORM_DRI_DRI_BUFFER_H_

#include "base/macros.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/ozone_export.h"

class SkCanvas;

namespace ui {

class DriWrapper;

// Wrapper for a DRM allocated buffer. Keeps track of the native properties of
// the buffer and wraps the pixel memory into a SkSurface which can be used to
// draw into using Skia.
class OZONE_EXPORT DriBuffer {
 public:
  DriBuffer(DriWrapper* dri);
  virtual ~DriBuffer();

  uint32_t stride() const { return stride_; }
  uint32_t handle() const { return handle_; }
  uint32_t framebuffer() const { return framebuffer_; }
  SkCanvas* canvas() { return surface_->getCanvas(); }

  // Allocates the backing pixels and wraps them in |surface_|. |info| is used
  // to describe the buffer characteristics (size, color format).
  virtual bool Initialize(const SkImageInfo& info);

 protected:
  DriWrapper* dri_;  // Not owned.

  // Wrapper around the native pixel memory.
  skia::RefPtr<SkSurface> surface_;

  // Length of a row of pixels.
  uint32_t stride_;

  // Buffer handle used by the DRM allocator.
  uint32_t handle_;

  // Buffer ID used by the DRM modesettings API. This is set when the buffer is
  // registered with the CRTC.
  uint32_t framebuffer_;

  DISALLOW_COPY_AND_ASSIGN(DriBuffer);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_BUFFER_H_
