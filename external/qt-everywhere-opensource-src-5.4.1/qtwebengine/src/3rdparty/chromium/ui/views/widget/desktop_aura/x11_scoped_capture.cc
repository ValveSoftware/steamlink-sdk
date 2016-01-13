// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_scoped_capture.h"

#include <X11/X.h>
#include <X11/Xlib.h>

#include "ui/gfx/x/x11_types.h"

namespace views {

X11ScopedCapture::X11ScopedCapture(XID window)
    : captured_(false) {
  // TODO(sad): Use XI2 API instead.
  unsigned int event_mask = PointerMotionMask | ButtonReleaseMask |
                            ButtonPressMask;
  int status = XGrabPointer(gfx::GetXDisplay(), window, True, event_mask,
                            GrabModeAsync, GrabModeAsync, None, None,
                            CurrentTime);
  captured_ = status == GrabSuccess;
}

X11ScopedCapture::~X11ScopedCapture() {
  if (captured_) {
    XUngrabPointer(gfx::GetXDisplay(), CurrentTime);
  }
}

}  // namespace views
