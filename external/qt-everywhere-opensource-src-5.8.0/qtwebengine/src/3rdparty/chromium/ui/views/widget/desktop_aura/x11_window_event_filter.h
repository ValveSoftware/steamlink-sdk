// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WINDOW_EVENT_FILTER_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WINDOW_EVENT_FILTER_H_

#include <X11/Xlib.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace views {
class DesktopWindowTreeHost;
class NativeWidgetAura;

// An EventFilter that sets properties on X11 windows.
class VIEWS_EXPORT X11WindowEventFilter : public ui::EventHandler {
 public:
  explicit X11WindowEventFilter(DesktopWindowTreeHost* window_tree_host);
  ~X11WindowEventFilter() override;

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  // Called when the user clicked the caption area.
  void OnClickedCaption(ui::MouseEvent* event,
                        int previous_click_component);

  // Called when the user clicked the maximize button.
  void OnClickedMaximizeButton(ui::MouseEvent* event);

  void ToggleMaximizedState();

  // Dispatches a _NET_WM_MOVERESIZE message to the window manager to tell it
  // to act as if a border or titlebar drag occurred.
  bool DispatchHostWindowDragMovement(int hittest,
                                      const gfx::Point& screen_location);

  // The display and the native X window hosting the root window.
  XDisplay* xdisplay_;
  ::Window xwindow_;

  // The native root window.
  ::Window x_root_window_;

  ui::X11AtomCache atom_cache_;

  DesktopWindowTreeHost* window_tree_host_;

  // The non-client component for the target of a MouseEvent. Mouse events can
  // be destructive to the window tree, which can cause the component of a
  // ui::EF_IS_DOUBLE_CLICK event to no longer be the same as that of the
  // initial click. Acting on a double click should only occur for matching
  // components.
  int click_component_;

  DISALLOW_COPY_AND_ASSIGN(X11WindowEventFilter);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WINDOW_EVENT_FILTER_H_
