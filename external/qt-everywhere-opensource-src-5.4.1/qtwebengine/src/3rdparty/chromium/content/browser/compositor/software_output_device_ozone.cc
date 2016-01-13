// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/software_output_device_ozone.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace content {

SoftwareOutputDeviceOzone::SoftwareOutputDeviceOzone(ui::Compositor* compositor)
    : compositor_(compositor) {
  ui::SurfaceFactoryOzone* factory = ui::SurfaceFactoryOzone::GetInstance();

  if (factory->InitializeHardware() != ui::SurfaceFactoryOzone::INITIALIZED)
    LOG(FATAL) << "Failed to initialize hardware in OZONE";

  surface_ozone_ = factory->CreateCanvasForWidget(compositor_->widget());

  if (!surface_ozone_)
    LOG(FATAL) << "Failed to initialize canvas";

  vsync_provider_ = surface_ozone_->CreateVSyncProvider();
}

SoftwareOutputDeviceOzone::~SoftwareOutputDeviceOzone() {
}

void SoftwareOutputDeviceOzone::Resize(const gfx::Size& viewport_pixel_size,
                                       float scale_factor) {
  scale_factor_ = scale_factor;

  if (viewport_pixel_size_ == viewport_pixel_size)
    return;

  viewport_pixel_size_ = viewport_pixel_size;

  surface_ozone_->ResizeCanvas(viewport_pixel_size_);
}

SkCanvas* SoftwareOutputDeviceOzone::BeginPaint(const gfx::Rect& damage_rect) {
  DCHECK(gfx::Rect(viewport_pixel_size_).Contains(damage_rect));

  // Get canvas for next frame.
  canvas_ = surface_ozone_->GetCanvas();

  return SoftwareOutputDevice::BeginPaint(damage_rect);
}

void SoftwareOutputDeviceOzone::EndPaint(cc::SoftwareFrameData* frame_data) {
  SoftwareOutputDevice::EndPaint(frame_data);

  surface_ozone_->PresentCanvas(damage_rect_);
}

}  // namespace content
