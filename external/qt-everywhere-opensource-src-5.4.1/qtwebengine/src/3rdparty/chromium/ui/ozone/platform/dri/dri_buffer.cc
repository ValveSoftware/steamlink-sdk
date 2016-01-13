// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_buffer.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <xf86drm.h>

#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

namespace {

// Modesetting cannot happen from a buffer with transparencies. Return the size
// of a pixel without alpha.
uint8_t GetColorDepth(SkColorType type) {
  switch (type) {
    case kUnknown_SkColorType:
    case kAlpha_8_SkColorType:
      return 0;
    case kIndex_8_SkColorType:
      return 8;
    case kRGB_565_SkColorType:
      return 16;
    case kARGB_4444_SkColorType:
      return 12;
    case kPMColor_SkColorType:
      return 24;
    default:
      NOTREACHED();
      return 0;
  }
}

void DestroyDumbBuffer(int fd, uint32_t handle) {
  struct drm_mode_destroy_dumb destroy_request;
  destroy_request.handle = handle;
  drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_request);
}

bool CreateDumbBuffer(DriWrapper* dri,
                      const SkImageInfo& info,
                      uint32_t* handle,
                      uint32_t* stride,
                      void** pixels) {
  struct drm_mode_create_dumb request;
  request.width = info.width();
  request.height = info.height();
  request.bpp = info.bytesPerPixel() << 3;
  request.flags = 0;

  if (drmIoctl(dri->get_fd(), DRM_IOCTL_MODE_CREATE_DUMB, &request) < 0) {
    DLOG(ERROR) << "Cannot create dumb buffer (" << errno << ") "
                << strerror(errno);
    return false;
  }

  *handle = request.handle;
  *stride = request.pitch;

  struct drm_mode_map_dumb map_request;
  map_request.handle = request.handle;
  if (drmIoctl(dri->get_fd(), DRM_IOCTL_MODE_MAP_DUMB, &map_request)) {
    DLOG(ERROR) << "Cannot prepare dumb buffer for mapping (" << errno << ") "
                << strerror(errno);
    DestroyDumbBuffer(dri->get_fd(), request.handle);
    return false;
  }

  *pixels = mmap(0,
                 request.size,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED,
                 dri->get_fd(),
                 map_request.offset);
  if (*pixels == MAP_FAILED) {
    DLOG(ERROR) << "Cannot mmap dumb buffer (" << errno << ") "
                << strerror(errno);
    DestroyDumbBuffer(dri->get_fd(), request.handle);
    return false;
  }

  return true;
}

}  // namespace

DriBuffer::DriBuffer(DriWrapper* dri)
    : dri_(dri), handle_(0), framebuffer_(0) {}

DriBuffer::~DriBuffer() {
  if (!surface_)
    return;

  if (framebuffer_)
    dri_->RemoveFramebuffer(framebuffer_);

  SkImageInfo info;
  void* pixels = const_cast<void*>(surface_->peekPixels(&info, NULL));
  if (!pixels)
    return;

  munmap(pixels, info.getSafeSize(stride_));
  DestroyDumbBuffer(dri_->get_fd(), handle_);
}

bool DriBuffer::Initialize(const SkImageInfo& info) {
  void* pixels = NULL;
  if (!CreateDumbBuffer(dri_, info, &handle_, &stride_, &pixels)) {
    DLOG(ERROR) << "Cannot allocate drm dumb buffer";
    return false;
  }

  if (!dri_->AddFramebuffer(info.width(),
                            info.height(),
                            GetColorDepth(info.colorType()),
                            info.bytesPerPixel() << 3,
                            stride_,
                            handle_,
                            &framebuffer_)) {
    DLOG(ERROR) << "Failed to register framebuffer: " << strerror(errno);
    return false;
  }

  surface_ = skia::AdoptRef(SkSurface::NewRasterDirect(info, pixels, stride_));
  if (!surface_) {
    DLOG(ERROR) << "Cannot install Skia pixels for drm buffer";
    return false;
  }

  return true;
}

}  // namespace ui
