// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_CURSOR_FACTORY_OZONE_H_
#define UI_OZONE_PUBLIC_CURSOR_FACTORY_OZONE_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_base_export.h"

namespace gfx {
class Point;
}

namespace ui {

typedef void* PlatformCursor;

class OZONE_BASE_EXPORT CursorFactoryOzone {
 public:
  CursorFactoryOzone();
  virtual ~CursorFactoryOzone();

  // Returns the singleton instance.
  static CursorFactoryOzone* GetInstance();

  // Return the default cursor of the specified type. The types are listed in
  // ui/base/cursor/cursor.h. Default cursors are managed by the implementation
  // and must live indefinitely; there's no way to know when to free them.
  virtual PlatformCursor GetDefaultCursor(int type);

  // Return a image cursor from the specified image & hotspot. Image cursors
  // are referenced counted and have an initial refcount of 1. Therefore, each
  // CreateImageCursor call must be matched with a call to UnrefImageCursor.
  virtual PlatformCursor CreateImageCursor(const SkBitmap& bitmap,
                                           const gfx::Point& hotspot);

  // Increment platform image cursor refcount.
  virtual void RefImageCursor(PlatformCursor cursor);

  // Decrement platform image cursor refcount.
  virtual void UnrefImageCursor(PlatformCursor cursor);

  // Change the active cursor for an AcceleratedWidget.
  // TODO(spang): Move this.
  virtual void SetCursor(gfx::AcceleratedWidget widget, PlatformCursor cursor);

  // Returns the window on which the cursor is active.
  // TODO(dnicoara) Move this once the WindowTreeHost refactoring finishes and
  // WindowTreeHost::CanDispatchEvent() is no longer present.
  virtual gfx::AcceleratedWidget GetCursorWindow();

 private:
  static CursorFactoryOzone* impl_;  // not owned
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_CURSOR_FACTORY_OZONE_H_
