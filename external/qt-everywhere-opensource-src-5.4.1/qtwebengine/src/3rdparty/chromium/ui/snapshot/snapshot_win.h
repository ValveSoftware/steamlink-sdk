// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SNAPSHOT_SNAPSHOT_WIN_H_
#define UI_SNAPSHOT_SNAPSHOT_WIN_H_

#include <windows.h>

#include <vector>

#include "ui/snapshot/snapshot_export.h"

namespace gfx {
class Rect;
}

namespace ui {
namespace internal {

// Grabs a snapshot of the desktop. No security checks are done.  This is
// intended to be used for debugging purposes where no BrowserProcess instance
// is available (ie. tests). DO NOT use in a result of user action.
SNAPSHOT_EXPORT bool GrabHwndSnapshot(
    HWND window_handle,
    const gfx::Rect& snapshot_bounds,
    std::vector<unsigned char>* png_representation);

}  // namespace internal
}  // namespace ui

#endif  // UI_SNAPSHOT_SNAPSHOT_WIN_H_
