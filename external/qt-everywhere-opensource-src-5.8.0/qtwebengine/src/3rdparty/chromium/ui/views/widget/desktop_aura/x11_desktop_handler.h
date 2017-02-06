// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_HANDLER_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_HANDLER_H_

#include <stdint.h>
#include <X11/Xlib.h>
// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include <vector>

#include "base/macros.h"
#include "ui/aura/env_observer.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/views_export.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace views {

// A singleton that owns global objects related to the desktop and listens for
// X11 events on the X11 root window. Destroys itself when aura::Env is
// deleted.
class VIEWS_EXPORT X11DesktopHandler : public ui::PlatformEventDispatcher,
                                       public aura::EnvObserver {
 public:
  // Returns the singleton handler.
  static X11DesktopHandler* get();

  // Gets/sets the X11 server time of the most recent mouse click, touch or
  // key press on a Chrome window.
  int wm_user_time_ms() const { return wm_user_time_ms_; }
  void set_wm_user_time_ms(Time time_ms);

  // Sends a request to the window manager to activate |window|.
  // This method should only be called if the window is already mapped.
  void ActivateWindow(::Window window);

  // Attempts to get the window manager to deactivate |window| by moving it to
  // the bottom of the stack. Regardless of whether |window| was actually
  // deactivated, sets the window as inactive in our internal state.
  void DeactivateWindow(::Window window);

  // Checks if the current active window is |window|.
  bool IsActiveWindow(::Window window) const;

  // Processes activation/focus related events. Some of these events are
  // dispatched to the X11 window dispatcher, and not to the X11 root-window
  // dispatcher. The window dispatcher sends these events to here.
  void ProcessXEvent(XEvent* event);

  // ui::PlatformEventDispatcher
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  // Overridden from aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;
  void OnWillDestroyEnv() override;

 private:
  enum ActiveState {
    ACTIVE,
    NOT_ACTIVE
  };

  X11DesktopHandler();
  ~X11DesktopHandler() override;

  // Handles changes in activation.
  void OnActiveWindowChanged(::Window window, ActiveState active_state);

  // Called when |window| has been created or destroyed. |window| may not be
  // managed by Chrome.
  void OnWindowCreatedOrDestroyed(int event_type, XID window);

  // The display and the native X window hosting the root window.
  XDisplay* xdisplay_;

  // The native root window.
  ::Window x_root_window_;

  // The last known active X window
  ::Window x_active_window_;

  // The X11 server time of the most recent mouse click, touch, or key press
  // on a Chrome window.
  Time wm_user_time_ms_;

  // The active window according to X11 server.
  ::Window current_window_;

  // Whether we should treat |current_window_| as active. In particular, we
  // pretend that a window is deactivated after a call to DeactivateWindow().
  ActiveState current_window_active_state_;

  ui::X11AtomCache atom_cache_;

  bool wm_supports_active_window_;

  DISALLOW_COPY_AND_ASSIGN(X11DesktopHandler);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_HANDLER_H_
