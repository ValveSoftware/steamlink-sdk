// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/surface_factory_ozone.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

// static
SurfaceFactoryOzone* SurfaceFactoryOzone::impl_ = NULL;

SurfaceFactoryOzone::SurfaceFactoryOzone() {
  CHECK(!impl_) << "There should only be a single SurfaceFactoryOzone.";
  impl_ = this;
}

SurfaceFactoryOzone::~SurfaceFactoryOzone() {
  CHECK_EQ(impl_, this);
  impl_ = NULL;
}

SurfaceFactoryOzone* SurfaceFactoryOzone::GetInstance() {
  CHECK(impl_) << "No SurfaceFactoryOzone implementation set.";
  return impl_;
}

intptr_t SurfaceFactoryOzone::GetNativeDisplay() {
  return 0;
}

scoped_ptr<SurfaceOzoneEGL> SurfaceFactoryOzone::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  NOTIMPLEMENTED();
  return scoped_ptr<SurfaceOzoneEGL>();
}

scoped_ptr<SurfaceOzoneCanvas> SurfaceFactoryOzone::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  NOTIMPLEMENTED();
  return scoped_ptr<SurfaceOzoneCanvas>();
}

const int32* SurfaceFactoryOzone::GetEGLSurfaceProperties(
    const int32* desired_attributes) {
  return desired_attributes;
}

ui::OverlayCandidatesOzone* SurfaceFactoryOzone::GetOverlayCandidates(
    gfx::AcceleratedWidget w) {
  return NULL;
}

void SurfaceFactoryOzone::ScheduleOverlayPlane(
    gfx::AcceleratedWidget w,
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    ui::NativeBufferOzone buffer,
    const gfx::Rect& display_bounds,
    gfx::RectF crop_rect) {
  NOTREACHED();
}

ui::NativeBufferOzone SurfaceFactoryOzone::CreateNativeBuffer(
    gfx::Size size,
    BufferFormat format) {
  return 0;
}

}  // namespace ui
