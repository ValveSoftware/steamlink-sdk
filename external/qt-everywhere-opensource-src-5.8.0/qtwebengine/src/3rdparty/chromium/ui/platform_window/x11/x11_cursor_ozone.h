// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_CURSOR_OZONE_H_
#define UI_PLATFORM_WINDOW_X11_X11_CURSOR_OZONE_H_

#include <X11/X.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/base/cursor/cursor.h"
#include "ui/platform_window/x11/x11_window_export.h"

class SkBitmap;

namespace ui {

// Ref counted class to hold an X11 cursor resource. Handles creating X11 cursor
// resources from SkBitmap/hotspot and clears the X11 resources on destruction.
class X11_WINDOW_EXPORT X11CursorOzone
    : public base::RefCounted<X11CursorOzone> {
 public:
  X11CursorOzone(const SkBitmap& bitmap, const gfx::Point& hotspot);
  X11CursorOzone(const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& hotspot,
                 int frame_delay_ms);

  // Creates a new cursor that is invisible.
  static scoped_refptr<X11CursorOzone> CreateInvisible();

  ::Cursor xcursor() const { return xcursor_; }

 private:
  friend class base::RefCounted<X11CursorOzone>;

  X11CursorOzone();
  ~X11CursorOzone();

  ::Cursor xcursor_ = None;

  DISALLOW_COPY_AND_ASSIGN(X11CursorOzone);
};

}  // namespace ui
#endif  // UI_PLATFORM_WINDOW_X11_X11_CURSOR_OZONE_H_
