// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/software_output_device_mac.h"

#include "base/mac/foundation_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gfx/skia_util.h"

namespace content {

SoftwareOutputDeviceMac::SoftwareOutputDeviceMac(ui::Compositor* compositor)
    : compositor_(compositor), scale_factor_(1), current_index_(0) {}

SoftwareOutputDeviceMac::~SoftwareOutputDeviceMac() {
}

void SoftwareOutputDeviceMac::Resize(const gfx::Size& pixel_size,
                                     float scale_factor) {
  if (pixel_size_ == pixel_size && scale_factor_ == scale_factor)
    return;

  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;

  DiscardBackbuffer();
}

void SoftwareOutputDeviceMac::CopyPreviousBufferDamage(
    const SkRegion& new_damage_region) {
  TRACE_EVENT0("browser", "CopyPreviousBufferDamage");

  // The region to copy is the region drawn last frame, minus the region that
  // is drawn this frame.
  SkRegion copy_region = previous_buffer_damage_region_;
  bool copy_region_nonempty =
      copy_region.op(new_damage_region, SkRegion::kDifference_Op);
  previous_buffer_damage_region_ = new_damage_region;

  if (!copy_region_nonempty)
    return;

  IOSurfaceRef previous_io_surface = io_surfaces_[!current_index_].get();

  {
    TRACE_EVENT0("browser", "IOSurfaceLock for software copy");
    IOReturn io_result = IOSurfaceLock(
        previous_io_surface, kIOSurfaceLockReadOnly | kIOSurfaceLockAvoidSync,
        nullptr);
    if (io_result) {
      DLOG(ERROR) << "Failed to lock previous IOSurface " << io_result;
      return;
    }
  }

  uint8_t* pixels =
      static_cast<uint8_t*>(IOSurfaceGetBaseAddress(previous_io_surface));
  size_t stride = IOSurfaceGetBytesPerRow(previous_io_surface);
  size_t bytes_per_element = 4;
  for (SkRegion::Iterator it(copy_region); !it.done(); it.next()) {
    const SkIRect& rect = it.rect();
    canvas_->writePixels(
        SkImageInfo::MakeN32Premul(rect.width(), rect.height()),
        pixels + bytes_per_element * rect.x() + stride * rect.y(), stride,
        rect.x(), rect.y());
  }

  {
    TRACE_EVENT0("browser", "IOSurfaceUnlock");
    IOReturn io_result = IOSurfaceUnlock(
        previous_io_surface, kIOSurfaceLockReadOnly | kIOSurfaceLockAvoidSync,
        nullptr);
    if (io_result)
      DLOG(ERROR) << "Failed to unlock previous IOSurface " << io_result;
  }
}

bool SoftwareOutputDeviceMac::EnsureBuffersExist() {
  for (int i = 0; i < 2; ++i) {
    if (!io_surfaces_[i]) {
      TRACE_EVENT0("browser", "IOSurfaceCreate");
      io_surfaces_[i].reset(
          gfx::CreateIOSurface(pixel_size_, gfx::BufferFormat::BGRA_8888));
    }
    if (!io_surfaces_[i]) {
      DLOG(ERROR) << "Failed to allocate IOSurface";
      return false;
    }
  }
  return true;
}

SkCanvas* SoftwareOutputDeviceMac::BeginPaint(
    const gfx::Rect& new_damage_rect) {
  if (!EnsureBuffersExist())
    return nullptr;

  {
    TRACE_EVENT0("browser", "IOSurfaceLock for software paint");
    IOReturn io_result = IOSurfaceLock(io_surfaces_[current_index_],
                                       kIOSurfaceLockAvoidSync, nullptr);
    if (io_result) {
      DLOG(ERROR) << "Failed to lock IOSurface " << io_result;
      return nullptr;
    }
  }

  SkPMColor* pixels = static_cast<SkPMColor*>(
      IOSurfaceGetBaseAddress(io_surfaces_[current_index_]));
  size_t stride = IOSurfaceGetBytesPerRow(io_surfaces_[current_index_]);

  canvas_ = sk_sp<SkCanvas>(SkCanvas::NewRasterDirectN32(
      pixel_size_.width(), pixel_size_.height(), pixels, stride));

  CopyPreviousBufferDamage(SkRegion(gfx::RectToSkIRect(new_damage_rect)));

  return canvas_.get();
}

void SoftwareOutputDeviceMac::EndPaint() {
  SoftwareOutputDevice::EndPaint();
  {
    TRACE_EVENT0("browser", "IOSurfaceUnlock");
    IOReturn io_result = IOSurfaceUnlock(io_surfaces_[current_index_],
                                         kIOSurfaceLockAvoidSync, nullptr);
    if (io_result)
      DLOG(ERROR) << "Failed to unlock IOSurface " << io_result;
  }
  canvas_.reset();

  ui::AcceleratedWidgetMac* widget =
      ui::AcceleratedWidgetMac::Get(compositor_->widget());
  if (widget) {
    widget->GotIOSurfaceFrame(io_surfaces_[current_index_], pixel_size_,
                              scale_factor_);
    base::TimeTicks vsync_timebase;
    base::TimeDelta vsync_interval;
    widget->GetVSyncParameters(&vsync_timebase, &vsync_interval);
    if (!update_vsync_callback_.is_null())
      update_vsync_callback_.Run(vsync_timebase, vsync_interval);
  }

  current_index_ = !current_index_;
}

void SoftwareOutputDeviceMac::DiscardBackbuffer() {
  for (int i = 0; i < 2; ++i)
    io_surfaces_[i].reset();
}

void SoftwareOutputDeviceMac::EnsureBackbuffer() {}

gfx::VSyncProvider* SoftwareOutputDeviceMac::GetVSyncProvider() {
  return this;
}

void SoftwareOutputDeviceMac::GetVSyncParameters(
    const gfx::VSyncProvider::UpdateVSyncCallback& callback) {
  update_vsync_callback_ = callback;
}

}  // namespace content
