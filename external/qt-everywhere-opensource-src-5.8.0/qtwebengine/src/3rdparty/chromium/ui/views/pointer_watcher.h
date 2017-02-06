// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_POINTER_WATCHER_H_
#define UI_VIEWS_POINTER_WATCHER_H_

#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace ui {
class MouseEvent;
class TouchEvent;
}

namespace views {
class Widget;

// An interface for read-only observation of pointer events (in particular, the
// events cannot be marked as handled). Only certain event types are supported.
// The |target| is the top-level widget that will receive the event, if any.
// NOTE: On mus this allows observation of events outside of windows owned
// by the current process, in which case the |target| will be null. On mus
// event.target() is always null.
class VIEWS_EXPORT PointerWatcher {
 public:
  virtual ~PointerWatcher() {}

  virtual void OnMousePressed(const ui::MouseEvent& event,
                              const gfx::Point& location_in_screen,
                              Widget* target) = 0;
  virtual void OnTouchPressed(const ui::TouchEvent& event,
                              const gfx::Point& location_in_screen,
                              Widget* target) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_POINTER_WATCHER_H_
