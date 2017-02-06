// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/ui/blimp_screen.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/size.h"

namespace blimp {
namespace engine {

namespace {

const int64_t kDisplayId = 1;
const int kNumDisplays = 1;

}  // namespace

BlimpScreen::BlimpScreen() : display_(kDisplayId) {}

BlimpScreen::~BlimpScreen() {}

void BlimpScreen::UpdateDisplayScaleAndSize(float scale,
                                            const gfx::Size& size) {
  if (scale == display_.device_scale_factor() &&
      size == display_.GetSizeInPixel()) {
    return;
  }

  uint32_t metrics = display::DisplayObserver::DISPLAY_METRIC_NONE;
  if (scale != display_.device_scale_factor())
    metrics |= display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;

  if (size != display_.GetSizeInPixel())
    metrics |= display::DisplayObserver::DISPLAY_METRIC_BOUNDS;

  display_.SetScaleAndBounds(scale, gfx::Rect(size));

  FOR_EACH_OBSERVER(display::DisplayObserver, observers_,
                    OnDisplayMetricsChanged(display_, metrics));
}

gfx::Point BlimpScreen::GetCursorScreenPoint() {
  return gfx::Point();
}

bool BlimpScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow BlimpScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return window_tree_host_
             ? window_tree_host_->window()->GetTopWindowContainingPoint(point)
             : gfx::NativeWindow(nullptr);
}

int BlimpScreen::GetNumDisplays() const {
  return kNumDisplays;
}

std::vector<display::Display> BlimpScreen::GetAllDisplays() const {
  return std::vector<display::Display>(1, display_);
}

display::Display BlimpScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return display_;
}

display::Display BlimpScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return display_;
}

display::Display BlimpScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return display_;
}

display::Display BlimpScreen::GetPrimaryDisplay() const {
  return display_;
}

void BlimpScreen::AddObserver(display::DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void BlimpScreen::RemoveObserver(display::DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace engine
}  // namespace blimp
