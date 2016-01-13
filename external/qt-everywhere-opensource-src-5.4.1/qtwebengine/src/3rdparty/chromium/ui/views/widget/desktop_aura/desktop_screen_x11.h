// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/screen.h"
#include "ui/views/views_export.h"

namespace gfx {
class Display;
}

typedef unsigned long XID;
typedef XID Window;
typedef struct _XDisplay Display;

namespace views {
class DesktopScreenX11Test;

// Our singleton screen implementation that talks to xrandr.
class VIEWS_EXPORT DesktopScreenX11 : public gfx::Screen,
                                      public ui::PlatformEventDispatcher {
 public:
  DesktopScreenX11();

  virtual ~DesktopScreenX11();

  // Takes a set of displays and dispatches the screen change events to
  // listeners. Exposed for testing.
  void ProcessDisplayChange(const std::vector<gfx::Display>& displays);

  // Overridden from gfx::Screen:
  virtual bool IsDIPEnabled() OVERRIDE;
  virtual gfx::Point GetCursorScreenPoint() OVERRIDE;
  virtual gfx::NativeWindow GetWindowUnderCursor() OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point)
      OVERRIDE;
  virtual int GetNumDisplays() const OVERRIDE;
  virtual std::vector<gfx::Display> GetAllDisplays() const OVERRIDE;
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView window) const OVERRIDE;
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE;
  virtual gfx::Display GetPrimaryDisplay() const OVERRIDE;
  virtual void AddObserver(gfx::DisplayObserver* observer) OVERRIDE;
  virtual void RemoveObserver(gfx::DisplayObserver* observer) OVERRIDE;

  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE;
  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE;

 private:
  friend class DesktopScreenX11Test;

  // Constructor used in tests.
  DesktopScreenX11(const std::vector<gfx::Display>& test_displays);

  // Builds a list of displays from the current screen information offered by
  // the X server.
  std::vector<gfx::Display> BuildDisplaysFromXRandRInfo();

  // We delay updating the display so we can coalesce events.
  void ConfigureTimerFired();

  Display* xdisplay_;
  ::Window x_root_window_;

  // Whether the x server supports the XRandR extension.
  bool has_xrandr_;

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_;

  // The display objects we present to chrome.
  std::vector<gfx::Display> displays_;

  // The timer to delay configuring outputs. See also the comments in
  // Dispatch().
  scoped_ptr<base::OneShotTimer<DesktopScreenX11> > configure_timer_;

  ObserverList<gfx::DisplayObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(DesktopScreenX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_
