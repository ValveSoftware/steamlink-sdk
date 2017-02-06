// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_screen.h"

#include <stdint.h>

#include "base/command_line.h"
#include "chromecast/public/graphics_properties_shlib.h"
#include "ui/aura/env.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"

namespace chromecast {

namespace {

const int64_t kDisplayId = 1;

// Helper to return the screen resolution (device pixels)
// to use.
gfx::Size GetScreenResolution() {
  if (GraphicsPropertiesShlib::IsSupported(
          GraphicsPropertiesShlib::k1080p,
          base::CommandLine::ForCurrentProcess()->argv())) {
    return gfx::Size(1920, 1080);
  } else {
    return gfx::Size(1280, 720);
  }
}

}  // namespace

CastScreen::~CastScreen() {
}

gfx::Point CastScreen::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

bool CastScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow CastScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return gfx::NativeWindow(nullptr);
}

int CastScreen::GetNumDisplays() const {
  return 1;
}

std::vector<display::Display> CastScreen::GetAllDisplays() const {
  return std::vector<display::Display>(1, display_);
}

display::Display CastScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return display_;
}

display::Display CastScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return display_;
}

display::Display CastScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return display_;
}

display::Display CastScreen::GetPrimaryDisplay() const {
  return display_;
}

void CastScreen::AddObserver(display::DisplayObserver* observer) {}

void CastScreen::RemoveObserver(display::DisplayObserver* observer) {}

CastScreen::CastScreen() : display_(kDisplayId) {
  // Device scale factor computed relative to 720p display
  const gfx::Size size = GetScreenResolution();
  const float device_scale_factor = size.height() / 720.0f;
  display_.SetScaleAndBounds(device_scale_factor, gfx::Rect(size));
}

}  // namespace chromecast
