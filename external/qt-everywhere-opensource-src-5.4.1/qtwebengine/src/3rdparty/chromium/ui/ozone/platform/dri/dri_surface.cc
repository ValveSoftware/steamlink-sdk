// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_surface.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <xf86drm.h>

#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

DriSurface::DriSurface(
    DriWrapper* dri, const gfx::Size& size)
    : dri_(dri),
      bitmaps_(),
      front_buffer_(0),
      size_(size) {
}

DriSurface::~DriSurface() {
}

bool DriSurface::Initialize() {
  for (size_t i = 0; i < arraysize(bitmaps_); ++i) {
    bitmaps_[i].reset(CreateBuffer());
    // TODO(dnicoara) Should select the configuration based on what the
    // underlying system supports.
    SkImageInfo info = SkImageInfo::Make(size_.width(),
                                         size_.height(),
                                         kPMColor_SkColorType,
                                         kPremul_SkAlphaType);
    if (!bitmaps_[i]->Initialize(info)) {
      return false;
    }
  }

  return true;
}

uint32_t DriSurface::GetFramebufferId() const {
  CHECK(backbuffer());
  return backbuffer()->framebuffer();
}

uint32_t DriSurface::GetHandle() const {
  CHECK(backbuffer());
  return backbuffer()->handle();
}

// This call is made after the hardware just started displaying our back buffer.
// We need to update our pointer reference and synchronize the two buffers.
void DriSurface::SwapBuffers() {
  CHECK(frontbuffer());
  CHECK(backbuffer());

  // Update our front buffer pointer.
  front_buffer_ ^= 1;
}

gfx::Size DriSurface::Size() const {
  return size_;
}

SkCanvas* DriSurface::GetDrawableForWidget() {
  CHECK(backbuffer());
  return backbuffer()->canvas();
}

DriBuffer* DriSurface::CreateBuffer() { return new DriBuffer(dri_); }

}  // namespace ui
