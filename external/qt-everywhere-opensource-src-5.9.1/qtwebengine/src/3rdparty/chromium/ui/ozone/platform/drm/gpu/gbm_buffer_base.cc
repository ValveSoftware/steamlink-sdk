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
                             uint32_t format,
                             uint32_t flags,
                             uint64_t modifier,
                             uint32_t addfb_flags)
    : drm_(drm), bo_(bo) {
  if (flags & GBM_BO_USE_SCANOUT) {
    DCHECK(bo_);
    // The framebuffer format might be different than the format:
    // drm supports 24 bit color depth and formats with alpha will
    // be converted to one without it.
    framebuffer_pixel_format_ =
        GetFourCCFormatForFramebuffer(GetBufferFormatFromFourCCFormat(format));

    // TODO(dcastagna): Add multi-planar support.
    uint32_t handles[4] = {0};
    handles[0] = gbm_bo_get_handle(bo).u32;
    uint32_t strides[4] = {0};
    strides[0] = gbm_bo_get_stride(bo);
    uint32_t offsets[4] = {0};
    uint64_t modifiers[4] = {0};
    modifiers[0] = modifier;

    // AddFramebuffer2 only considers the modifiers if addfb_flags has
    // DRM_MODE_FB_MODIFIERS set. We only set that when we've created
    // a bo with modifiers, otherwise, we rely on the "no modifiers"
    // behavior doing the right thing.
    if (!drm_->AddFramebuffer2(gbm_bo_get_width(bo), gbm_bo_get_height(bo),
                               framebuffer_pixel_format_, handles, strides,
                               offsets, modifiers, &framebuffer_,
                               addfb_flags)) {
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
