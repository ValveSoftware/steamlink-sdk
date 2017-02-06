// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_screen.h"

#include <stdint.h>

#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"

namespace headless {

namespace {

bool IsRotationPortrait(display::Display::Rotation rotation) {
  return rotation == display::Display::ROTATE_90 ||
         rotation == display::Display::ROTATE_270;
}

}  // namespace

// static
HeadlessScreen* HeadlessScreen::Create(const gfx::Size& size) {
  const gfx::Size kDefaultSize(800, 600);
  return new HeadlessScreen(gfx::Rect(size.IsEmpty() ? kDefaultSize : size));
}

HeadlessScreen::~HeadlessScreen() {}

aura::WindowTreeHost* HeadlessScreen::CreateHostForPrimaryDisplay() {
  DCHECK(!host_);
  host_ = aura::WindowTreeHost::Create(gfx::Rect(display_.GetSizeInPixel()));
  // Some tests don't correctly manage window focus/activation states.
  // Makes sure InputMethod is default focused so that IME basics can work.
  host_->GetInputMethod()->OnFocus();
  host_->window()->AddObserver(this);
  host_->InitHost();
  return host_;
}

void HeadlessScreen::SetDeviceScaleFactor(float device_scale_factor) {
  gfx::Rect bounds_in_pixel(display_.GetSizeInPixel());
  display_.SetScaleAndBounds(device_scale_factor, bounds_in_pixel);
}

void HeadlessScreen::SetDisplayRotation(display::Display::Rotation rotation) {
  gfx::Rect bounds_in_pixel(display_.GetSizeInPixel());
  gfx::Rect new_bounds(bounds_in_pixel);
  if (IsRotationPortrait(rotation) != IsRotationPortrait(display_.rotation())) {
    new_bounds.set_width(bounds_in_pixel.height());
    new_bounds.set_height(bounds_in_pixel.width());
  }
  display_.set_rotation(rotation);
  display_.SetScaleAndBounds(display_.device_scale_factor(), new_bounds);
  host_->SetRootTransform(GetRotationTransform() * GetUIScaleTransform());
}

void HeadlessScreen::SetUIScale(float ui_scale) {
  ui_scale_ = ui_scale;
  gfx::Rect bounds_in_pixel(display_.GetSizeInPixel());
  gfx::Rect new_bounds = gfx::ToNearestRect(
      gfx::ScaleRect(gfx::RectF(bounds_in_pixel), 1.0f / ui_scale));
  display_.SetScaleAndBounds(display_.device_scale_factor(), new_bounds);
  host_->SetRootTransform(GetRotationTransform() * GetUIScaleTransform());
}

void HeadlessScreen::SetWorkAreaInsets(const gfx::Insets& insets) {
  display_.UpdateWorkAreaFromInsets(insets);
}

gfx::Transform HeadlessScreen::GetRotationTransform() const {
  gfx::Transform rotate;
  switch (display_.rotation()) {
    case display::Display::ROTATE_0:
      break;
    case display::Display::ROTATE_90:
      rotate.Translate(display_.bounds().height(), 0);
      rotate.Rotate(90);
      break;
    case display::Display::ROTATE_270:
      rotate.Translate(0, display_.bounds().width());
      rotate.Rotate(270);
      break;
    case display::Display::ROTATE_180:
      rotate.Translate(display_.bounds().width(), display_.bounds().height());
      rotate.Rotate(180);
      break;
  }

  return rotate;
}

gfx::Transform HeadlessScreen::GetUIScaleTransform() const {
  gfx::Transform ui_scale;
  ui_scale.Scale(1.0f / ui_scale_, 1.0f / ui_scale_);
  return ui_scale;
}

void HeadlessScreen::OnWindowBoundsChanged(aura::Window* window,
                                           const gfx::Rect& old_bounds,
                                           const gfx::Rect& new_bounds) {
  DCHECK_EQ(host_->window(), window);
  display_.SetSize(gfx::ScaleToFlooredSize(new_bounds.size(),
                                           display_.device_scale_factor()));
}

void HeadlessScreen::OnWindowDestroying(aura::Window* window) {
  if (host_->window() == window)
    host_ = NULL;
}

gfx::Point HeadlessScreen::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

bool HeadlessScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow HeadlessScreen::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  if (!host_ || !host_->window())
    return nullptr;
  return host_->window()->GetTopWindowContainingPoint(point);
}

int HeadlessScreen::GetNumDisplays() const {
  return 1;
}

std::vector<display::Display> HeadlessScreen::GetAllDisplays() const {
  return std::vector<display::Display>(1, display_);
}

display::Display HeadlessScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return display_;
}

display::Display HeadlessScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return display_;
}

display::Display HeadlessScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return display_;
}

display::Display HeadlessScreen::GetPrimaryDisplay() const {
  return display_;
}

void HeadlessScreen::AddObserver(display::DisplayObserver* observer) {}

void HeadlessScreen::RemoveObserver(display::DisplayObserver* observer) {}

HeadlessScreen::HeadlessScreen(const gfx::Rect& screen_bounds)
    : host_(NULL), ui_scale_(1.0f) {
  static int64_t synthesized_display_id = 2000;
  display_.set_id(synthesized_display_id++);
  display_.SetScaleAndBounds(1.0f, screen_bounds);
}

}  // namespace headless
