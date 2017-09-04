// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window_manager_ozone.h"

#include <X11/Xlib.h>

namespace ui {

X11WindowManagerOzone::X11WindowManagerOzone() : event_grabber_(None) {}

X11WindowManagerOzone::~X11WindowManagerOzone() {}

void X11WindowManagerOzone::GrabEvents(XID xwindow) {
  if (event_grabber_ != None)
    return;
  event_grabber_ = xwindow;
}

void X11WindowManagerOzone::UngrabEvents(XID xwindow) {
  if (event_grabber_ != xwindow)
    return;
  event_grabber_ = None;
}

}  // namespace ui
