// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_SURFACE_H_
#define UI_OZONE_PLATFORM_DRI_DRI_SURFACE_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/platform/dri/scanout_surface.h"

class SkCanvas;

namespace ui {

class DriBuffer;
class DriWrapper;

// An implementation of ScanoutSurface which uses dumb buffers (used for
// software rendering).
class OZONE_EXPORT DriSurface : public ScanoutSurface {
 public:
  DriSurface(DriWrapper* dri, const gfx::Size& size);
  virtual ~DriSurface();

  // Get a Skia canvas for a backbuffer.
  SkCanvas* GetDrawableForWidget();

  // ScanoutSurface:
  virtual bool Initialize() OVERRIDE;
  virtual uint32_t GetFramebufferId() const OVERRIDE;
  virtual uint32_t GetHandle() const OVERRIDE;
  virtual void SwapBuffers() OVERRIDE;
  virtual gfx::Size Size() const OVERRIDE;

 private:
  DriBuffer* frontbuffer() const { return bitmaps_[front_buffer_].get(); }
  DriBuffer* backbuffer() const { return bitmaps_[front_buffer_ ^ 1].get(); }

  // Used to create the backing buffers.
  virtual DriBuffer* CreateBuffer();

  // Stores the connection to the graphics card. Pointer not owned by this
  // class.
  DriWrapper* dri_;

  // The actual buffers used for painting.
  scoped_ptr<DriBuffer> bitmaps_[2];

  // Keeps track of which bitmap is |buffers_| is the frontbuffer.
  int front_buffer_;

  // Surface size.
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(DriSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_SURFACE_H_
