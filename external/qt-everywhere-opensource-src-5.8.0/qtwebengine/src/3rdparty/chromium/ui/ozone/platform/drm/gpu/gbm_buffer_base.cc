// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_buffer_base.h"

#include <gbm.h>

#include "base/logging.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/gbm_device.h"

namespace ui {

GbmBufferBase::GbmBufferBase(const scoped_refptr<GbmDevice>& drm,
                             gbm_bo* bo,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage)
    : drm_(drm), bo_(bo) {
  if (usage == gfx::BufferUsage::SCANOUT) {
    framebuffer_pixel_format_ = GetFourCCFormatForFramebuffer(format);

    uint32_t handles[4] = {0};
    handles[0] = gbm_bo_get_handle(bo).u32;
    uint32_t strides[4] = {0};
    strides[0] = gbm_bo_get_stride(bo);
    uint32_t offsets[4] = {0};

    if (!drm_->AddFramebuffer2(gbm_bo_get_width(bo), gbm_bo_get_height(bo),
                               framebuffer_pixel_format_, handles, strides,
                               offsets, &framebuffer_, 0)) {
      PLOG(ERROR) << "Failed to register buffer";
      return;
    }
  }
}

GbmBufferBase::~GbmBufferBase() {
  if (framebuffer_)
    drm_->RemoveFramebuffer(framebuffer_);
}

uint32_t GbmBufferBase::GetFramebufferId() const {
  return framebuffer_;
}

uint32_t GbmBufferBase::GetHandle() const {
  return gbm_bo_get_handle(bo_).u32;
}

gfx::Size GbmBufferBase::GetSize() const {
  return gfx::Size(gbm_bo_get_width(bo_), gbm_bo_get_height(bo_));
}

uint32_t GbmBufferBase::GetFramebufferPixelFormat() const {
  DCHECK(framebuffer_);
  return framebuffer_pixel_format_;
}

const DrmDevice* GbmBufferBase::GetDrmDevice() const {
  return drm_.get();
}

bool GbmBufferBase::RequiresGlFinish() const {
  return !drm_->is_primary_device();
}

}  // namespace ui
