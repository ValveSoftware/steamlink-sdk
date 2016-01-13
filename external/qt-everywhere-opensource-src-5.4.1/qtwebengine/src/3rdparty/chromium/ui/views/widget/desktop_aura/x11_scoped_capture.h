// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_SCOPED_CAPTURE_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_SCOPED_CAPTURE_H_

#include "base/basictypes.h"

typedef unsigned long XID;

namespace views {

// Grabs X11 input on a window., i.e. all subsequent mouse events are dispatched
// to the window, while the capture is active.
class X11ScopedCapture {
 public:
  explicit X11ScopedCapture(XID window);
  virtual ~X11ScopedCapture();

 private:
  bool captured_;

  DISALLOW_COPY_AND_ASSIGN(X11ScopedCapture);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_SCOPED_CAPTURE_H_
