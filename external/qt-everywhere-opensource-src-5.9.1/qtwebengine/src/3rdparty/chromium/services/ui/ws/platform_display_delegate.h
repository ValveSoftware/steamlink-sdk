// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_PLATFORM_DISPLAY_DELEGATE_H_
#define SERVICES_UI_WS_PLATFORM_DISPLAY_DELEGATE_H_

namespace gfx {
class Size;
}

namespace ui {

class Event;

namespace ws {

class ServerWindow;

// PlatformDisplayDelegate is implemented by an object that manages the
// lifetime of a PlatformDisplay, forwards events to the appropriate windows,
/// and responses to changes in viewport size.
class PlatformDisplayDelegate {
 public:
  // Returns the root window of this display.
  virtual ServerWindow* GetRootWindow() = 0;

  // Called once when the AcceleratedWidget is available for drawing.
  virtual void OnAcceleratedWidgetAvailable() = 0;

  virtual bool IsInHighContrastMode() = 0;

  // Called when an event arrives.
  virtual void OnEvent(const ui::Event& event) = 0;

  // Called when the Display loses capture.
  virtual void OnNativeCaptureLost() = 0;

 protected:
  virtual ~PlatformDisplayDelegate() {}
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_PLATFORM_DISPLAY_DELEGATE_H_
