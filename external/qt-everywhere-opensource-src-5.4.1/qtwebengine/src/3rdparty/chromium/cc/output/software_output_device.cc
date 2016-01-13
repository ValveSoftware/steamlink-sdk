// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/software_output_device.h"

#include "base/logging.h"
#include "cc/output/software_frame_data.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"

namespace cc {

SoftwareOutputDevice::SoftwareOutputDevice() : scale_factor_(1.f) {
}

SoftwareOutputDevice::~SoftwareOutputDevice() {}

void SoftwareOutputDevice::Resize(const gfx::Size& viewport_pixel_size,
                                  float scale_factor) {
  scale_factor_ = scale_factor;

  if (viewport_pixel_size_ == viewport_pixel_size)
    return;

  SkImageInfo info = SkImageInfo::MakeN32(viewport_pixel_size.width(),
                                          viewport_pixel_size.height(),
                                          kOpaque_SkAlphaType);
  viewport_pixel_size_ = viewport_pixel_size;
  canvas_ = skia::AdoptRef(SkCanvas::NewRaster(info));
}

SkCanvas* SoftwareOutputDevice::BeginPaint(const gfx::Rect& damage_rect) {
  DCHECK(canvas_);
  damage_rect_ = damage_rect;
  return canvas_.get();
}

void SoftwareOutputDevice::EndPaint(SoftwareFrameData* frame_data) {
  DCHECK(frame_data);
  frame_data->id = 0;
  frame_data->size = viewport_pixel_size_;
  frame_data->damage_rect = damage_rect_;
}

void SoftwareOutputDevice::CopyToPixels(const gfx::Rect& rect, void* pixels) {
  DCHECK(canvas_);
  SkImageInfo info = SkImageInfo::MakeN32Premul(rect.width(), rect.height());
  canvas_->readPixels(info, pixels, info.minRowBytes(), rect.x(), rect.y());
}

void SoftwareOutputDevice::Scroll(const gfx::Vector2d& delta,
                                  const gfx::Rect& clip_rect) {
  NOTIMPLEMENTED();
}

void SoftwareOutputDevice::ReclaimSoftwareFrame(unsigned id) {
  NOTIMPLEMENTED();
}

gfx::VSyncProvider* SoftwareOutputDevice::GetVSyncProvider() {
  return vsync_provider_.get();
}

}  // namespace cc
