// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"

#include <drm.h>
#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>
#include <utility>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_pixmap_handle_ozone.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/gbm_device.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

GbmBuffer::GbmBuffer(const scoped_refptr<GbmDevice>& gbm,
                     gbm_bo* bo,
                     gfx::BufferFormat format,
                     gfx::BufferUsage usage,
                     std::vector<base::ScopedFD>&& fds,
                     const gfx::Size& size,
                     const std::vector<int>& strides,
                     const std::vector<int>& offsets)
    : GbmBufferBase(gbm, bo, format, usage),
      format_(format),
      usage_(usage),
      fds_(std::move(fds)),
      size_(size),
      strides_(strides),
      offsets_(offsets) {}

GbmBuffer::~GbmBuffer() {
  if (bo())
    gbm_bo_destroy(bo());
}

bool GbmBuffer::AreFdsValid() const {
  if (fds_.empty())
    return false;

  for (const auto& fd : fds_) {
    if (fd.get() == -1)
      return false;
  }
  return true;
}

size_t GbmBuffer::GetFdCount() const {
  return fds_.size();
}

int GbmBuffer::GetFd(size_t plane) const {
  DCHECK_LT(plane, fds_.size());
  return fds_[plane].get();
}

int GbmBuffer::GetStride(size_t plane) const {
  DCHECK_LT(plane, strides_.size());
  return strides_[plane];
}

int GbmBuffer::GetOffset(size_t plane) const {
  DCHECK_LT(plane, offsets_.size());
  return offsets_[plane];
}

// TODO(reveman): This should not be needed once crbug.com/597932 is fixed,
// as the size would be queried directly from the underlying bo.
gfx::Size GbmBuffer::GetSize() const {
  return size_;
}

// static
scoped_refptr<GbmBuffer> GbmBuffer::CreateBuffer(
    const scoped_refptr<GbmDevice>& gbm,
    gfx::BufferFormat format,
    const gfx::Size& size,
    gfx::BufferUsage usage) {
  TRACE_EVENT2("drm", "GbmBuffer::CreateBuffer", "device",
               gbm->device_path().value(), "size", size.ToString());

  unsigned flags = 0;
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      break;
    case gfx::BufferUsage::SCANOUT:
      flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
      break;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      flags = GBM_BO_USE_LINEAR;
      break;
  }

  gbm_bo* bo = gbm_bo_create(gbm->device(), size.width(), size.height(),
                             GetFourCCFormatFromBufferFormat(format), flags);
  if (!bo)
    return nullptr;

  // The fd returned by gbm_bo_get_fd is not ref-counted and need to be
  // kept open for the lifetime of the buffer.
  base::ScopedFD fd(gbm_bo_get_fd(bo));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to export buffer to dma_buf";
    gbm_bo_destroy(bo);
    return nullptr;
  }
  std::vector<base::ScopedFD> fds;
  fds.emplace_back(std::move(fd));
  std::vector<int> strides;
  strides.push_back(gbm_bo_get_stride(bo));
  std::vector<int> offsets;
  offsets.push_back(gbm_bo_get_plane_offset(bo, 0));
  scoped_refptr<GbmBuffer> buffer(new GbmBuffer(
      gbm, bo, format, usage, std::move(fds), size, strides, offsets));
  if (usage == gfx::BufferUsage::SCANOUT && !buffer->GetFramebufferId())
    return nullptr;

  return buffer;
}

// static
scoped_refptr<GbmBuffer> GbmBuffer::CreateBufferFromFds(
    const scoped_refptr<GbmDevice>& gbm,
    gfx::BufferFormat format,
    const gfx::Size& size,
    std::vector<base::ScopedFD>&& fds,
    const std::vector<int>& strides,
    const std::vector<int>& offsets) {
  TRACE_EVENT2("drm", "GbmBuffer::CreateBufferFromFD", "device",
               gbm->device_path().value(), "size", size.ToString());
  DCHECK_LE(fds.size(), strides.size());
  DCHECK_EQ(strides.size(), offsets.size());
  DCHECK_EQ(offsets[0], 0);

  uint32_t fourcc_format = GetFourCCFormatFromBufferFormat(format);

  // Use scanout if supported.
  bool use_scanout = gbm_device_is_format_supported(
      gbm->device(), fourcc_format, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

  gbm_bo* bo = nullptr;
  if (use_scanout) {
    struct gbm_import_fd_data fd_data;
    fd_data.fd = fds[0].get();
    fd_data.width = size.width();
    fd_data.height = size.height();
    fd_data.stride = strides[0];
    fd_data.format = fourcc_format;

    // The fd passed to gbm_bo_import is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    bo = gbm_bo_import(gbm->device(), GBM_BO_IMPORT_FD, &fd_data,
                       GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!bo) {
      LOG(ERROR) << "nullptr returned from gbm_bo_import";
      return nullptr;
    }
  }

  scoped_refptr<GbmBuffer> buffer(new GbmBuffer(
      gbm, bo, format,
      use_scanout ? gfx::BufferUsage::SCANOUT : gfx::BufferUsage::GPU_READ,
      std::move(fds), size, strides, offsets));
  // If scanout support for buffer is expected then make sure we managed to
  // create a framebuffer for it as otherwise using it for scanout will fail.
  if (use_scanout && !buffer->GetFramebufferId())
    return nullptr;

  return buffer;
}

GbmPixmap::GbmPixmap(GbmSurfaceFactory* surface_manager,
                     const scoped_refptr<GbmBuffer>& buffer)
    : surface_manager_(surface_manager), buffer_(buffer) {}

void GbmPixmap::SetProcessingCallback(
    const ProcessingCallback& processing_callback) {
  DCHECK(processing_callback_.is_null());
  processing_callback_ = processing_callback;
}

gfx::NativePixmapHandle GbmPixmap::ExportHandle() {
  gfx::NativePixmapHandle handle;
  for (size_t i = 0;
       i < gfx::NumberOfPlanesForBufferFormat(buffer_->GetFormat()); ++i) {
    // Some formats (e.g: YVU_420) might have less than one fd per plane.
    if (i < buffer_->GetFdCount()) {
      base::ScopedFD scoped_fd(HANDLE_EINTR(dup(buffer_->GetFd(i))));
      if (!scoped_fd.is_valid()) {
        PLOG(ERROR) << "dup";
        return gfx::NativePixmapHandle();
      }
      handle.fds.emplace_back(
          base::FileDescriptor(scoped_fd.release(), true /* auto_close */));
    }
    handle.strides_and_offsets.emplace_back(buffer_->GetStride(i),
                                            buffer_->GetOffset(i));
  }
  return handle;
}

GbmPixmap::~GbmPixmap() {
}

void* GbmPixmap::GetEGLClientBuffer() const {
  return nullptr;
}

bool GbmPixmap::AreDmaBufFdsValid() const {
  return buffer_->AreFdsValid();
}

size_t GbmPixmap::GetDmaBufFdCount() const {
  return buffer_->GetFdCount();
}

int GbmPixmap::GetDmaBufFd(size_t plane) const {
  return buffer_->GetFd(plane);
}

int GbmPixmap::GetDmaBufPitch(size_t plane) const {
  return buffer_->GetStride(plane);
}

int GbmPixmap::GetDmaBufOffset(size_t plane) const {
  return buffer_->GetOffset(plane);
}

gfx::BufferFormat GbmPixmap::GetBufferFormat() const {
  return buffer_->GetFormat();
}

gfx::Size GbmPixmap::GetBufferSize() const {
  return buffer_->GetSize();
}

bool GbmPixmap::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                     int plane_z_order,
                                     gfx::OverlayTransform plane_transform,
                                     const gfx::Rect& display_bounds,
                                     const gfx::RectF& crop_rect) {
  DCHECK(buffer_->GetUsage() == gfx::BufferUsage::SCANOUT);
  OverlayPlane::ProcessBufferCallback processing_callback;
  if (!processing_callback_.is_null())
    processing_callback = base::Bind(&GbmPixmap::ProcessBuffer, this);

  surface_manager_->GetSurface(widget)->QueueOverlayPlane(
      OverlayPlane(buffer_, plane_z_order, plane_transform, display_bounds,
                   crop_rect, processing_callback));

  return true;
}

scoped_refptr<ScanoutBuffer> GbmPixmap::ProcessBuffer(const gfx::Size& size,
                                                      uint32_t format) {
  DCHECK(GetBufferSize() != size ||
         buffer_->GetFramebufferPixelFormat() != format);

  if (!processed_pixmap_ || size != processed_pixmap_->GetBufferSize() ||
      format != processed_pixmap_->buffer()->GetFramebufferPixelFormat()) {
    // Release any old processed pixmap.
    processed_pixmap_ = nullptr;
    gfx::BufferFormat buffer_format = GetBufferFormatFromFourCCFormat(format);

    scoped_refptr<GbmBuffer> buffer = GbmBuffer::CreateBuffer(
        buffer_->drm().get(), buffer_format, size, buffer_->GetUsage());
    if (!buffer)
      return nullptr;

    // ProcessBuffer is called on DrmThread. We could have used
    // CreateNativePixmap to initialize the pixmap, however it posts a
    // synchronous task to DrmThread resulting in a deadlock.
    processed_pixmap_ = new GbmPixmap(surface_manager_, buffer);
  }

  DCHECK(!processing_callback_.is_null());
  if (!processing_callback_.Run(this, processed_pixmap_)) {
    LOG(ERROR) << "Failed processing NativePixmap";
    return nullptr;
  }

  return processed_pixmap_->buffer();
}

}  // namespace ui
