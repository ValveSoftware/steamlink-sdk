// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_CURSOR_FACTORY_EVDEV_DRI_H_
#define UI_OZONE_PLATFORM_DRI_CURSOR_FACTORY_EVDEV_DRI_H_

#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class DriSurfaceFactory;
}

namespace ui {

class CursorFactoryEvdevDri : public BitmapCursorFactoryOzone,
                              public CursorDelegateEvdev {
 public:
  CursorFactoryEvdevDri(DriSurfaceFactory* dri);
  virtual ~CursorFactoryEvdevDri();

  // BitmapCursorFactoryOzone:
  virtual gfx::AcceleratedWidget GetCursorWindow() OVERRIDE;
  virtual void SetBitmapCursor(gfx::AcceleratedWidget widget,
                               scoped_refptr<BitmapCursorOzone> cursor)
      OVERRIDE;

  // CursorDelegateEvdev:
  virtual void MoveCursorTo(gfx::AcceleratedWidget widget,
                            const gfx::PointF& location) OVERRIDE;
  virtual void MoveCursor(const gfx::Vector2dF& delta) OVERRIDE;
  virtual gfx::PointF location() OVERRIDE;

 private:
  // The location of the bitmap (the cursor location is the hotspot location).
  gfx::Point bitmap_location();

  // The DRI implementation for setting the hardware cursor.
  DriSurfaceFactory* dri_;

  // The current cursor bitmap.
  scoped_refptr<BitmapCursorOzone> cursor_;

  // The window under the cursor.
  gfx::AcceleratedWidget cursor_window_;

  // The location of the cursor within the window.
  gfx::PointF cursor_location_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_CURSOR_FACTORY_EVDEV_DRI_H_
