// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/hardware_display_controller.h"

#include <drm.h>
#include <errno.h>
#include <string.h>
#include <xf86drm.h>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/scanout_surface.h"

namespace ui {

namespace {

// DRM callback on page flip events. This callback is triggered after the
// page flip has happened and the backbuffer is now the new frontbuffer
// The old frontbuffer is no longer used by the hardware and can be used for
// future draw operations.
//
// |device| will contain a reference to the |ScanoutSurface| object which
// the event belongs to.
//
// TODO(dnicoara) When we have a FD handler for the DRM calls in the message
// loop, we can move this function in the handler.
void HandlePageFlipEvent(int fd,
                         unsigned int frame,
                         unsigned int seconds,
                         unsigned int useconds,
                         void* controller) {
  TRACE_EVENT0("dri", "HandlePageFlipEvent");
  static_cast<HardwareDisplayController*>(controller)
      ->OnPageFlipEvent(frame, seconds, useconds);
}

}  // namespace

HardwareDisplayController::HardwareDisplayController(
    DriWrapper* drm,
    uint32_t connector_id,
    uint32_t crtc_id)
    : drm_(drm),
      connector_id_(connector_id),
      crtc_id_(crtc_id),
      surface_(),
      time_of_last_flip_(0),
      is_disabled_(true) {}

HardwareDisplayController::~HardwareDisplayController() {
  // Reset the cursor.
  UnsetCursor();
  UnbindSurfaceFromController();
}

bool
HardwareDisplayController::BindSurfaceToController(
    scoped_ptr<ScanoutSurface> surface, drmModeModeInfo mode) {
  CHECK(surface);

  if (!drm_->SetCrtc(crtc_id_,
                     surface->GetFramebufferId(),
                     &connector_id_,
                     &mode)) {
    LOG(ERROR) << "Failed to modeset: error='" << strerror(errno)
               << "' crtc=" << crtc_id_ << " connector=" << connector_id_
               << " framebuffer_id=" << surface->GetFramebufferId()
               << " mode=" << mode.hdisplay << "x" << mode.vdisplay << "@"
               << mode.vrefresh;
    return false;
  }

  surface_.reset(surface.release());
  mode_ = mode;
  is_disabled_ = false;
  return true;
}

void HardwareDisplayController::UnbindSurfaceFromController() {
  drm_->SetCrtc(crtc_id_, 0, 0, NULL);
  surface_.reset();
  memset(&mode_, 0, sizeof(mode_));
  is_disabled_ = true;
}

bool HardwareDisplayController::Enable() {
  CHECK(surface_);
  if (is_disabled_) {
    scoped_ptr<ScanoutSurface> surface(surface_.release());
    return BindSurfaceToController(surface.Pass(), mode_);
  }

  return true;
}

void HardwareDisplayController::Disable() {
  drm_->SetCrtc(crtc_id_, 0, 0, NULL);
  is_disabled_ = true;
}

bool HardwareDisplayController::SchedulePageFlip() {
  CHECK(surface_);
  if (!is_disabled_ && !drm_->PageFlip(crtc_id_,
                                       surface_->GetFramebufferId(),
                                       this)) {
    LOG(ERROR) << "Cannot page flip: " << strerror(errno);
    return false;
  }

  return true;
}

void HardwareDisplayController::WaitForPageFlipEvent() {
  TRACE_EVENT0("dri", "WaitForPageFlipEvent");

  if (is_disabled_)
    return;

  drmEventContext drm_event;
  drm_event.version = DRM_EVENT_CONTEXT_VERSION;
  drm_event.page_flip_handler = HandlePageFlipEvent;
  drm_event.vblank_handler = NULL;

  // Wait for the page-flip to complete.
  drm_->HandleEvent(drm_event);
}

void HardwareDisplayController::OnPageFlipEvent(unsigned int frame,
                                                unsigned int seconds,
                                                unsigned int useconds) {
  time_of_last_flip_ =
      static_cast<uint64_t>(seconds) * base::Time::kMicrosecondsPerSecond +
      useconds;

  surface_->SwapBuffers();
}

bool HardwareDisplayController::SetCursor(ScanoutSurface* surface) {
  bool ret = drm_->SetCursor(crtc_id_,
                             surface->GetHandle(),
                             surface->Size().width(),
                             surface->Size().height());
  surface->SwapBuffers();
  return ret;
}

bool HardwareDisplayController::UnsetCursor() {
  return drm_->SetCursor(crtc_id_, 0, 0, 0);
}

bool HardwareDisplayController::MoveCursor(const gfx::Point& location) {
  if (is_disabled_)
    return true;

  return drm_->MoveCursor(crtc_id_, location.x(), location.y());
}

}  // namespace ui
