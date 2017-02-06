// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window_ozone.h"

#include <X11/Xlib.h>

#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/point.h"
#include "ui/platform_window/x11/x11_cursor_ozone.h"
#include "ui/platform_window/x11/x11_window_manager_ozone.h"

namespace ui {

X11WindowOzone::X11WindowOzone(X11EventSourceLibevent* event_source,
                               X11WindowManagerOzone* window_manager,
                               PlatformWindowDelegate* delegate)
    : X11WindowBase(delegate),
      event_source_(event_source),
      window_manager_(window_manager) {
  DCHECK(event_source_);
  DCHECK(window_manager);
  event_source_->AddPlatformEventDispatcher(this);
  event_source_->AddXEventDispatcher(this);
}

X11WindowOzone::~X11WindowOzone() {
  event_source_->RemovePlatformEventDispatcher(this);
  event_source_->RemoveXEventDispatcher(this);
}

void X11WindowOzone::SetCapture() {
  window_manager_->GrabEvents(xwindow());
}

void X11WindowOzone::ReleaseCapture() {
  window_manager_->UngrabEvents(xwindow());
}

void X11WindowOzone::SetCursor(PlatformCursor cursor) {
  X11CursorOzone* cursor_ozone = static_cast<X11CursorOzone*>(cursor);
  XDefineCursor(xdisplay(), xwindow(), cursor_ozone->xcursor());
}

bool X11WindowOzone::DispatchXEvent(XEvent* xev) {
  if (!IsEventForXWindow(*xev))
    return false;

  ProcessXWindowEvent(xev);
  return true;
}

bool X11WindowOzone::CanDispatchEvent(const PlatformEvent& platform_event) {
  if (xwindow() == None)
    return false;

  // If there is a grab, capture events here.
  XID grabber = window_manager_->event_grabber();
  if (grabber != None)
    return grabber == xwindow();

  // TODO(kylechar): We may need to do something special for TouchEvents similar
  // to how DrmWindowHost handles them.
  if (static_cast<Event*>(platform_event)->IsLocatedEvent()) {
    const LocatedEvent* event =
        static_cast<const LocatedEvent*>(platform_event);
    return GetBounds().Contains(event->root_location());
  }

  return true;
}

uint32_t X11WindowOzone::DispatchEvent(const PlatformEvent& platform_event) {
  // This is unfortunately needed otherwise events that depend on global state
  // (eg. double click) are broken.
  DispatchEventFromNativeUiEvent(
      platform_event, base::Bind(&PlatformWindowDelegate::DispatchEvent,
                                 base::Unretained(delegate())));
  return POST_DISPATCH_STOP_PROPAGATION;
}

}  // namespace ui
