// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_surface_factory.h"

#include <errno.h>

#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/dri_vsync_provider.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

namespace {

// TODO(dnicoara) Read the cursor plane size from the hardware.
const gfx::Size kCursorSize(64, 64);

void UpdateCursorImage(DriSurface* cursor, const SkBitmap& image) {
  SkRect damage;
  image.getBounds(&damage);

  // Clear to transparent in case |image| is smaller than the canvas.
  SkCanvas* canvas = cursor->GetDrawableForWidget();
  canvas->clear(SK_ColorTRANSPARENT);

  SkRect clip;
  clip.set(
      0, 0, canvas->getDeviceSize().width(), canvas->getDeviceSize().height());
  canvas->clipRect(clip, SkRegion::kReplace_Op);
  canvas->drawBitmapRectToRect(image, &damage, damage);
}

class DriSurfaceAdapter : public ui::SurfaceOzoneCanvas {
 public:
  DriSurfaceAdapter(const base::WeakPtr<HardwareDisplayController>& controller);
  virtual ~DriSurfaceAdapter();

  // SurfaceOzoneCanvas:
  virtual skia::RefPtr<SkCanvas> GetCanvas() OVERRIDE;
  virtual void ResizeCanvas(const gfx::Size& viewport_size) OVERRIDE;
  virtual void PresentCanvas(const gfx::Rect& damage) OVERRIDE;
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() OVERRIDE;

 private:
  void UpdateNativeSurface(const gfx::Rect& damage);

  skia::RefPtr<SkSurface> surface_;
  gfx::Rect last_damage_;
  base::WeakPtr<HardwareDisplayController> controller_;

  DISALLOW_COPY_AND_ASSIGN(DriSurfaceAdapter);
};

DriSurfaceAdapter::DriSurfaceAdapter(
    const base::WeakPtr<HardwareDisplayController>& controller)
    : controller_(controller) {
}

DriSurfaceAdapter::~DriSurfaceAdapter() {
}

skia::RefPtr<SkCanvas> DriSurfaceAdapter::GetCanvas() {
  return skia::SharePtr(surface_->getCanvas());
}

void DriSurfaceAdapter::ResizeCanvas(const gfx::Size& viewport_size) {
  SkImageInfo info = SkImageInfo::MakeN32(
      viewport_size.width(), viewport_size.height(), kOpaque_SkAlphaType);
  surface_ = skia::AdoptRef(SkSurface::NewRaster(info));
}

void DriSurfaceAdapter::PresentCanvas(const gfx::Rect& damage) {
  CHECK(base::MessageLoopForUI::IsCurrent());
  if (!controller_)
    return;

  UpdateNativeSurface(damage);
  controller_->SchedulePageFlip();
  controller_->WaitForPageFlipEvent();
}

scoped_ptr<gfx::VSyncProvider> DriSurfaceAdapter::CreateVSyncProvider() {
  return scoped_ptr<gfx::VSyncProvider>(new DriVSyncProvider(controller_));
}

void DriSurfaceAdapter::UpdateNativeSurface(const gfx::Rect& damage) {
  SkCanvas* canvas = static_cast<DriSurface*>(controller_->surface())
      ->GetDrawableForWidget();

  // The DriSurface is double buffered, so the current back buffer is
  // missing the previous update. Expand damage region.
  SkRect real_damage = RectToSkRect(UnionRects(damage, last_damage_));

  // Copy damage region.
  skia::RefPtr<SkImage> image = skia::AdoptRef(surface_->newImageSnapshot());
  image->draw(canvas, &real_damage, real_damage, NULL);

  last_damage_ = damage;
}

}  // namespace

// static
const gfx::AcceleratedWidget DriSurfaceFactory::kDefaultWidgetHandle = 1;

DriSurfaceFactory::DriSurfaceFactory(DriWrapper* drm,
                                     ScreenManager* screen_manager)
    : drm_(drm),
      screen_manager_(screen_manager),
      state_(UNINITIALIZED),
      allocated_widgets_(0) {
}

DriSurfaceFactory::~DriSurfaceFactory() {
  if (state_ == INITIALIZED)
    ShutdownHardware();
}

ui::SurfaceFactoryOzone::HardwareState DriSurfaceFactory::InitializeHardware() {
  if (state_ != UNINITIALIZED)
    return state_;

  if (drm_->get_fd() < 0) {
    LOG(ERROR) << "Failed to create DRI connection";
    state_ = FAILED;
    return state_;
  }

  cursor_surface_.reset(CreateSurface(kCursorSize));
  if (!cursor_surface_->Initialize()) {
    LOG(ERROR) << "Failed to initialize cursor surface";
    state_ = FAILED;
    return state_;
  }

  state_ = INITIALIZED;
  return state_;
}

void DriSurfaceFactory::ShutdownHardware() {
  CHECK(state_ == INITIALIZED);
  state_ = UNINITIALIZED;
}

gfx::AcceleratedWidget DriSurfaceFactory::GetAcceleratedWidget() {
  CHECK(state_ != FAILED);

  // We're not using 0 since other code assumes that a 0 AcceleratedWidget is an
  // invalid widget.
  return ++allocated_widgets_;
}

scoped_ptr<ui::SurfaceOzoneCanvas> DriSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget w) {
  CHECK(state_ == INITIALIZED);
  // Initial cursor set.
  ResetCursor(w);

  return scoped_ptr<ui::SurfaceOzoneCanvas>(
      new DriSurfaceAdapter(screen_manager_->GetDisplayController(w)));
}

bool DriSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

gfx::Size DriSurfaceFactory::GetWidgetSize(gfx::AcceleratedWidget w) {
  base::WeakPtr<HardwareDisplayController> controller =
      screen_manager_->GetDisplayController(w);
  if (controller)
    return gfx::Size(controller->get_mode().hdisplay,
                     controller->get_mode().vdisplay);

  return gfx::Size(0, 0);
}

void DriSurfaceFactory::SetHardwareCursor(gfx::AcceleratedWidget window,
                                          const SkBitmap& image,
                                          const gfx::Point& location) {
  cursor_bitmap_ = image;
  cursor_location_ = location;

  if (state_ != INITIALIZED)
    return;

  ResetCursor(window);
}

void DriSurfaceFactory::MoveHardwareCursor(gfx::AcceleratedWidget window,
                                           const gfx::Point& location) {
  cursor_location_ = location;

  if (state_ != INITIALIZED)
    return;

  base::WeakPtr<HardwareDisplayController> controller =
      screen_manager_->GetDisplayController(window);
  if (controller)
    controller->MoveCursor(location);
}

void DriSurfaceFactory::UnsetHardwareCursor(gfx::AcceleratedWidget window) {
  cursor_bitmap_.reset();

  if (state_ != INITIALIZED)
    return;

  ResetCursor(window);
}

////////////////////////////////////////////////////////////////////////////////
// DriSurfaceFactory private

DriSurface* DriSurfaceFactory::CreateSurface(const gfx::Size& size) {
  return new DriSurface(drm_, size);
}

void DriSurfaceFactory::ResetCursor(gfx::AcceleratedWidget w) {
  base::WeakPtr<HardwareDisplayController> controller =
      screen_manager_->GetDisplayController(w);

  if (!cursor_bitmap_.empty()) {
    // Draw new cursor into backbuffer.
    UpdateCursorImage(cursor_surface_.get(), cursor_bitmap_);

    // Reset location & buffer.
    if (controller) {
      controller->MoveCursor(cursor_location_);
      controller->SetCursor(cursor_surface_.get());
    }
  } else {
    // No cursor set.
    if (controller)
      controller->UnsetCursor();
  }
}

}  // namespace ui
