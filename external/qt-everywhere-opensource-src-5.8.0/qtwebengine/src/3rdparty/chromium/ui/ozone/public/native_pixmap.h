// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_NATIVE_PIXMAP_H_
#define UI_OZONE_PUBLIC_NATIVE_PIXMAP_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_pixmap_handle_ozone.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {
class Rect;
class RectF;
}

namespace ui {

// This represents a buffer that can be directly imported via GL for
// rendering, or exported via dma-buf fds.
class NativePixmap : public base::RefCountedThreadSafe<NativePixmap> {
 public:
  NativePixmap() {}

  virtual void* /* EGLClientBuffer */ GetEGLClientBuffer() const = 0;
  virtual bool AreDmaBufFdsValid() const = 0;
  virtual size_t GetDmaBufFdCount() const = 0;
  virtual int GetDmaBufFd(size_t plane) const = 0;
  virtual int GetDmaBufPitch(size_t plane) const = 0;
  virtual int GetDmaBufOffset(size_t plane) const = 0;
  virtual gfx::BufferFormat GetBufferFormat() const = 0;
  virtual gfx::Size GetBufferSize() const = 0;

  // Sets the overlay plane to switch to at the next page flip.
  // |w| specifies the screen to display this overlay plane on.
  // |plane_z_order| specifies the stacking order of the plane relative to the
  // main framebuffer located at index 0.
  // |plane_transform| specifies how the buffer is to be transformed during.
  // composition.
  // |buffer| to be presented by the overlay.
  // |display_bounds| specify where it is supposed to be on the screen.
  // |crop_rect| specifies the region within the buffer to be placed
  // inside |display_bounds|. This is specified in texture coordinates, in the
  // range of [0,1].
  virtual bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                    int plane_z_order,
                                    gfx::OverlayTransform plane_transform,
                                    const gfx::Rect& display_bounds,
                                    const gfx::RectF& crop_rect) = 0;

  // This represents a callback function pointing to processing unit like VPP to
  // do post-processing operations like scaling and color space conversion on
  // |source_pixmap| and save processed result to |target_pixmap|.
  typedef base::Callback<bool(const scoped_refptr<NativePixmap>& source_pixmap,
                              scoped_refptr<NativePixmap> target_pixmap)>
      ProcessingCallback;

  // Set callback function for the pixmap used for post processing.
  virtual void SetProcessingCallback(
      const ProcessingCallback& processing_callback) = 0;

  // Export the buffer for sharing across processes.
  // Any file descriptors in the exported handle are owned by the caller.
  virtual gfx::NativePixmapHandle ExportHandle() = 0;

 protected:
  virtual ~NativePixmap() {}

 private:
  friend class base::RefCountedThreadSafe<NativePixmap>;

  DISALLOW_COPY_AND_ASSIGN(NativePixmap);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_NATIVE_PIXMAP_H_
