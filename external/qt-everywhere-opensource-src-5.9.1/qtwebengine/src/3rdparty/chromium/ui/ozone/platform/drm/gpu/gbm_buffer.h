// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/gpu/gbm_buffer_base.h"
#include "ui/ozone/public/native_pixmap.h"

struct gbm_bo;

namespace ui {

class GbmDevice;
class GbmSurfaceFactory;

class GbmBuffer : public GbmBufferBase {
 public:
  static scoped_refptr<GbmBuffer> CreateBuffer(
      const scoped_refptr<GbmDevice>& gbm,
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags);
  static scoped_refptr<GbmBuffer> CreateBufferWithModifiers(
      const scoped_refptr<GbmDevice>& gbm,
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags,
      const std::vector<uint64_t>& modifiers);
  static scoped_refptr<GbmBuffer> CreateBufferFromFds(
      const scoped_refptr<GbmDevice>& gbm,
      uint32_t format,
      const gfx::Size& size,
      std::vector<base::ScopedFD>&& fds,
      const std::vector<gfx::NativePixmapPlane>& planes);
  uint32_t GetFormat() const { return format_; }
  uint32_t GetFlags() const { return flags_; }
  bool AreFdsValid() const;
  size_t GetFdCount() const;
  int GetFd(size_t plane) const;
  int GetStride(size_t plane) const;
  int GetOffset(size_t plane) const;
  size_t GetSize(size_t plane) const;
  uint64_t GetFormatModifier(size_t plane) const;
  gfx::Size GetSize() const override;

 private:
  GbmBuffer(const scoped_refptr<GbmDevice>& gbm,
            gbm_bo* bo,
            uint32_t format,
            uint32_t flags,
            uint64_t modifier,
            uint32_t addfb_flags,
            std::vector<base::ScopedFD>&& fds,
            const gfx::Size& size,
            const std::vector<gfx::NativePixmapPlane>&& planes);
  ~GbmBuffer() override;

  static scoped_refptr<GbmBuffer> CreateBufferForBO(
      const scoped_refptr<GbmDevice>& gbm,
      gbm_bo* bo,
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags,
      uint64_t modifiers,
      uint32_t addfb_flags);

  uint32_t format_;
  uint32_t flags_;
  std::vector<base::ScopedFD> fds_;
  gfx::Size size_;

  std::vector<gfx::NativePixmapPlane> planes_;

  DISALLOW_COPY_AND_ASSIGN(GbmBuffer);
};

class GbmPixmap : public NativePixmap {
 public:
  GbmPixmap(GbmSurfaceFactory* surface_manager,
            const scoped_refptr<GbmBuffer>& buffer);

  void SetProcessingCallback(
      const ProcessingCallback& processing_callback) override;

  // NativePixmap:
  void* GetEGLClientBuffer() const override;
  bool AreDmaBufFdsValid() const override;
  size_t GetDmaBufFdCount() const override;
  int GetDmaBufFd(size_t plane) const override;
  int GetDmaBufPitch(size_t plane) const override;
  int GetDmaBufOffset(size_t plane) const override;
  uint64_t GetDmaBufModifier(size_t plane) const override;
  gfx::BufferFormat GetBufferFormat() const override;
  gfx::Size GetBufferSize() const override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect) override;
  gfx::NativePixmapHandle ExportHandle() override;

  scoped_refptr<GbmBuffer> buffer() { return buffer_; }

 private:
  ~GbmPixmap() override;
  scoped_refptr<ScanoutBuffer> ProcessBuffer(const gfx::Size& size,
                                             uint32_t format);

  GbmSurfaceFactory* surface_manager_;
  scoped_refptr<GbmBuffer> buffer_;

  // OverlayValidator can request scaling or format conversions as needed for
  // this Pixmap. This holds the processed buffer.
  scoped_refptr<GbmPixmap> processed_pixmap_;
  ProcessingCallback processing_callback_;

  DISALLOW_COPY_AND_ASSIGN(GbmPixmap);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_
