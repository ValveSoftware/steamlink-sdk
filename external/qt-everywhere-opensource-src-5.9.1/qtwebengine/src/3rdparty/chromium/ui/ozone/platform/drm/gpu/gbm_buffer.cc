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
#include "ui/gfx/native_pixmap_handle.h"
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
                     uint32_t format,
                     uint32_t flags,
                     uint64_t modifier,
                     uint32_t addfb_flags,
                     std::vector<base::ScopedFD>&& fds,
                     const gfx::Size& size,

                     const std::vector<gfx::NativePixmapPlane>&& planes)
    : GbmBufferBase(gbm, bo, format, flags, modifier, addfb_flags),
      format_(format),
      flags_(flags),
      fds_(std::move(fds)),
      size_(size),
      planes_(std::move(planes)) {}

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

int GbmBuffer::GetFd(size_t index) const {
  DCHECK_LT(index, fds_.size());
  return fds_[index].get();
}

int GbmBuffer::GetStride(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].stride;
}

int GbmBuffer::GetOffset(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].offset;
}

size_t GbmBuffer::GetSize(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].size;
}

uint64_t GbmBuffer::GetFormatModifier(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].modifier;
}

// TODO(reveman): This should not be needed once crbug.com/597932 is fixed,
// as the size would be queried directly from the underlying bo.
gfx::Size GbmBuffer::GetSize() const {
  return size_;
}

scoped_refptr<GbmBuffer> GbmBuffer::CreateBufferForBO(
    const scoped_refptr<GbmDevice>& gbm,
    gbm_bo* bo,
    uint32_t format,
    const gfx::Size& size,
    uint32_t flags,
    uint64_t modifier,
    uint32_t addfb_flags) {
  if (!bo)
    return nullptr;

  std::vector<base::ScopedFD> fds;
  std::vector<gfx::NativePixmapPlane> planes;

  for (size_t i = 0; i < gbm_bo_get_num_planes(bo); ++i) {
    // The fd returned by gbm_bo_get_fd is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    base::ScopedFD fd(gbm_bo_get_plane_fd(bo, i));

    // TODO(dcastagna): support multiple fds.
    // crbug.com/642410
    if (!i) {
      if (!fd.is_valid()) {
        PLOG(ERROR) << "Failed to export buffer to dma_buf";
        gbm_bo_destroy(bo);
        return nullptr;
      }
      fds.emplace_back(std::move(fd));
    }

    planes.emplace_back(
        gbm_bo_get_plane_stride(bo, i), gbm_bo_get_plane_offset(bo, i),
        gbm_bo_get_plane_size(bo, i), gbm_bo_get_plane_format_modifier(bo, i));
  }
  scoped_refptr<GbmBuffer> buffer(
      new GbmBuffer(gbm, bo, format, flags, modifier, addfb_flags,
                    std::move(fds), size, std::move(planes)));
  if (flags & GBM_BO_USE_SCANOUT && !buffer->GetFramebufferId())
    return nullptr;

  return buffer;
}

// static
scoped_refptr<GbmBuffer> GbmBuffer::CreateBufferWithModifiers(
    const scoped_refptr<GbmDevice>& gbm,
    uint32_t format,
    const gfx::Size& size,
    uint32_t flags,
    const std::vector<uint64_t>& modifiers) {
  TRACE_EVENT2("drm", "GbmBuffer::CreateBufferWithModifiers", "device",
               gbm->device_path().value(), "size", size.ToString());

  gbm_bo* bo =
      gbm_bo_create_with_modifiers(gbm->device(), size.width(), size.height(),
                                   format, modifiers.data(), modifiers.size());

  return CreateBufferForBO(gbm, bo, format, size, flags,
                           gbm_bo_get_format_modifier(bo),
                           DRM_MODE_FB_MODIFIERS);
}

// static
scoped_refptr<GbmBuffer> GbmBuffer::CreateBuffer(
    const scoped_refptr<GbmDevice>& gbm,
    uint32_t format,
    const gfx::Size& size,
    uint32_t flags) {
  TRACE_EVENT2("drm", "GbmBuffer::CreateBuffer", "device",
               gbm->device_path().value(), "size", size.ToString());

  gbm_bo* bo =
      gbm_bo_create(gbm->device(), size.width(), size.height(), format, flags);

  return CreateBufferForBO(gbm, bo, format, size, flags, 0, 0);
}

// static
scoped_refptr<GbmBuffer> GbmBuffer::CreateBufferFromFds(
    const scoped_refptr<GbmDevice>& gbm,
    uint32_t format,
    const gfx::Size& size,
    std::vector<base::ScopedFD>&& fds,
    const std::vector<gfx::NativePixmapPlane>& planes) {
  TRACE_EVENT2("drm", "GbmBuffer::CreateBufferFromFD", "device",
               gbm->device_path().value(), "size", size.ToString());
  DCHECK_LE(fds.size(), planes.size());
  DCHECK_EQ(planes[0].offset, 0);

  // Use scanout if supported.
  bool use_scanout =
      gbm_device_is_format_supported(
          gbm->device(), format, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING) &&
      (planes.size() == 1);

  gbm_bo* bo = nullptr;
  if (use_scanout) {
    struct gbm_import_fd_data fd_data;
    fd_data.fd = fds[0].get();
    fd_data.width = size.width();
    fd_data.height = size.height();
    fd_data.stride = planes[0].stride;
    fd_data.format = format;

    // The fd passed to gbm_bo_import is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    bo = gbm_bo_import(gbm->device(), GBM_BO_IMPORT_FD, &fd_data,
                       GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!bo) {
      LOG(ERROR) << "nullptr returned from gbm_bo_import";
      return nullptr;
    }
  }

  uint32_t flags = GBM_BO_USE_RENDERING;
  if (use_scanout)
    flags |= GBM_BO_USE_SCANOUT;
  scoped_refptr<GbmBuffer> buffer(new GbmBuffer(
      gbm, bo, format, flags, 0, 0, std::move(fds), size, std::move(planes)));
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
  gfx::BufferFormat format =
      ui::GetBufferFormatFromFourCCFormat(buffer_->GetFormat());
  // TODO(dcastagna): Use gbm_bo_get_num_planes once all the formats we use are
  // supported by gbm.
  for (size_t i = 0; i < gfx::NumberOfPlanesForBufferFormat(format); ++i) {
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
    handle.planes.emplace_back(buffer_->GetStride(i), buffer_->GetOffset(i),
                               buffer_->GetSize(i),
                               buffer_->GetFormatModifier(i));
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

uint64_t GbmPixmap::GetDmaBufModifier(size_t plane) const {
  return buffer_->GetFormatModifier(plane);
}

gfx::BufferFormat GbmPixmap::GetBufferFormat() const {
  return ui::GetBufferFormatFromFourCCFormat(buffer_->GetFormat());
}

gfx::Size GbmPixmap::GetBufferSize() const {
  return buffer_->GetSize();
}

bool GbmPixmap::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                     int plane_z_order,
                                     gfx::OverlayTransform plane_transform,
                                     const gfx::Rect& display_bounds,
                                     const gfx::RectF& crop_rect) {
  DCHECK(buffer_->GetFlags() & GBM_BO_USE_SCANOUT);
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
    scoped_refptr<GbmBuffer> buffer = GbmBuffer::CreateBuffer(
        buffer_->drm().get(), format, size, buffer_->GetFlags());
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
