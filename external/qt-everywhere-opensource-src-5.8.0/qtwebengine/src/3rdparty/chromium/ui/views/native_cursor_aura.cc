// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/native_cursor.h"

#include "ui/base/cursor/cursor.h"

namespace views {

gfx::NativeCursor GetNativeIBeamCursor() {
  return ui::kCursorIBeam;
}

gfx::NativeCursor GetNativeHandCursor() {
  return ui::kCursorHand;
}

gfx::NativeCursor GetNativeColumnResizeCursor() {
  return ui::kCursorColumnResize;
}

gfx::NativeCursor GetNativeEastWestResizeCursor() {
  return ui::kCursorEastWestResize;
}

gfx::NativeCursor GetNativeNorthSouthResizeCursor() {
  return ui::kCursorNorthSouthResize;
}

}  // namespace views
