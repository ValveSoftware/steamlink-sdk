// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include "base/logging.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/display.h"
#include "ui/gfx/size_conversions.h"

namespace gfx {

class ScreenAndroid : public Screen {
 public:
  ScreenAndroid() {}

  virtual bool IsDIPEnabled() OVERRIDE { return true; }

  virtual gfx::Point GetCursorScreenPoint() OVERRIDE { return gfx::Point(); }

  virtual gfx::NativeWindow GetWindowUnderCursor() OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point)
      OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  virtual gfx::Display GetPrimaryDisplay() const OVERRIDE {
    gfx::DeviceDisplayInfo device_info;
    const float device_scale_factor = device_info.GetDIPScale();
    const gfx::Rect bounds_in_pixels =
        gfx::Rect(
            device_info.GetDisplayWidth(),
            device_info.GetDisplayHeight());
    const gfx::Rect bounds_in_dip =
        gfx::Rect(gfx::ToCeiledSize(gfx::ScaleSize(
            bounds_in_pixels.size(), 1.0f / device_scale_factor)));
    gfx::Display display(0, bounds_in_dip);
    if (!gfx::Display::HasForceDeviceScaleFactor())
      display.set_device_scale_factor(device_scale_factor);
    display.SetRotationAsDegree(device_info.GetRotationDegrees());
    return display;
  }

  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView view) const OVERRIDE {
    return GetPrimaryDisplay();
  }

  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE {
    return GetPrimaryDisplay();
  }

  virtual int GetNumDisplays() const OVERRIDE { return 1; }

  virtual std::vector<gfx::Display> GetAllDisplays() const OVERRIDE {
    return std::vector<gfx::Display>(1, GetPrimaryDisplay());
  }

  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE {
    return GetPrimaryDisplay();
  }

  virtual void AddObserver(DisplayObserver* observer) OVERRIDE {
    // no display change on Android.
  }

  virtual void RemoveObserver(DisplayObserver* observer) OVERRIDE {
    // no display change on Android.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenAndroid);
};

Screen* CreateNativeScreen() {
  return new ScreenAndroid;
}

}  // namespace gfx
