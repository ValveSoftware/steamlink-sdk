// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_WINDOW_OZONE_H_
#define UI_PLATFORM_WINDOW_X11_X11_WINDOW_OZONE_H_

#include "base/macros.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source_libevent.h"
#include "ui/platform_window/x11/x11_window_base.h"
#include "ui/platform_window/x11/x11_window_export.h"

namespace ui {

class X11WindowManagerOzone;

// PlatformWindow implementation for X11 Ozone. PlatformEvents are ui::Events.
class X11_WINDOW_EXPORT X11WindowOzone : public X11WindowBase,
                                         public PlatformEventDispatcher,
                                         public XEventDispatcher {
 public:
  X11WindowOzone(X11EventSourceLibevent* event_source,
                 X11WindowManagerOzone* window_manager,
                 PlatformWindowDelegate* delegate);
  ~X11WindowOzone() override;

  // PlatformWindow:
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetCursor(PlatformCursor cursor) override;

  // XEventDispatcher:
  bool DispatchXEvent(XEvent* event) override;

 private:
  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  X11EventSourceLibevent* event_source_;
  X11WindowManagerOzone* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(X11WindowOzone);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_X11_WINDOW_OZONE_H_
