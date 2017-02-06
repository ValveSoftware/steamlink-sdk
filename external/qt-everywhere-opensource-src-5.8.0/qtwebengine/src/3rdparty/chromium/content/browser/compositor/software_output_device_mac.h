// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MAC_H_

#include <IOSurface/IOSurface.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "cc/output/software_output_device.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/vsync_provider.h"

class SkCanvas;

namespace gfx {
class Canvas;
}

namespace ui {
class Compositor;
}

namespace content {

class SoftwareOutputDeviceMac :
    public cc::SoftwareOutputDevice,
    public gfx::VSyncProvider {
 public:
  explicit SoftwareOutputDeviceMac(ui::Compositor* compositor);
  ~SoftwareOutputDeviceMac() override;

  // cc::SoftwareOutputDevice implementation.
  void Resize(const gfx::Size& pixel_size, float scale_factor) override;
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override;
  void EndPaint() override;
  void DiscardBackbuffer() override;
  void EnsureBackbuffer() override;
  gfx::VSyncProvider* GetVSyncProvider() override;

  // gfx::VSyncProvider implementation.
  void GetVSyncParameters(
      const gfx::VSyncProvider::UpdateVSyncCallback& callback) override;

 private:
  bool EnsureBuffersExist();

  // Copy the pixels from the previous buffer to the new buffer.
  void CopyPreviousBufferDamage(const SkRegion& new_damage_rect);

  ui::Compositor* compositor_;
  gfx::Size pixel_size_;
  float scale_factor_;

  // This surface is double-buffered. The two buffers are in |io_surfaces_|,
  // and the index of the current buffer is |current_buffer_|.
  base::ScopedCFTypeRef<IOSurfaceRef> io_surfaces_[2];
  int current_index_;

  // The previous frame's damage rectangle. Used to copy unchanged content
  // between buffers in CopyPreviousBufferDamage.
  SkRegion previous_buffer_damage_region_;

  // The SkCanvas wrapps the mapped current IOSurface. It is valid only between
  // BeginPaint and EndPaint.
  sk_sp<SkCanvas> canvas_;

  gfx::VSyncProvider::UpdateVSyncCallback update_vsync_callback_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MAC_H_
