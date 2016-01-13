// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_OZONE_BITMAP_CURSOR_FACTORY_OZONE_H_
#define UI_BASE_CURSOR_OZONE_BITMAP_CURSOR_FACTORY_OZONE_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/public/cursor_factory_ozone.h"

namespace ui {

// A cursor that is an SkBitmap combined with a gfx::Point hotspot.
class UI_BASE_EXPORT BitmapCursorOzone
    : public base::RefCounted<BitmapCursorOzone> {
 public:
  BitmapCursorOzone(const SkBitmap& bitmap, const gfx::Point& hotspot)
      : bitmap_(bitmap), hotspot_(hotspot) {}

  const gfx::Point& hotspot() { return hotspot_; }
  const SkBitmap& bitmap() { return bitmap_; }

 private:
  friend class base::RefCounted<BitmapCursorOzone>;
  ~BitmapCursorOzone() {}

  SkBitmap bitmap_;
  gfx::Point hotspot_;

  DISALLOW_COPY_AND_ASSIGN(BitmapCursorOzone);
};

// CursorFactoryOzone implementation for bitmapped cursors.
//
// This is a base class for platforms where PlatformCursor is an SkBitmap
// combined with a gfx::Point for the hotspot.
//
// Subclasses need only implement SetBitmapCursor() as everything else is
// implemented here.
class UI_BASE_EXPORT BitmapCursorFactoryOzone : public CursorFactoryOzone {
 public:
  BitmapCursorFactoryOzone();
  virtual ~BitmapCursorFactoryOzone();

  // CursorFactoryOzone:
  virtual PlatformCursor GetDefaultCursor(int type) OVERRIDE;
  virtual PlatformCursor CreateImageCursor(const SkBitmap& bitmap,
                                           const gfx::Point& hotspot) OVERRIDE;
  virtual void RefImageCursor(PlatformCursor cursor) OVERRIDE;
  virtual void UnrefImageCursor(PlatformCursor cursor) OVERRIDE;
  virtual void SetCursor(gfx::AcceleratedWidget widget,
                         PlatformCursor cursor) OVERRIDE;

  // Set a bitmap cursor for the given window. This must be overridden by
  // subclasses. If the cursor is hidden (kCursorNone) then cursor is NULL.
  virtual void SetBitmapCursor(gfx::AcceleratedWidget window,
                               scoped_refptr<BitmapCursorOzone> cursor);

 private:
  // Default cursors are cached & owned by the factory.
  typedef std::map<int, scoped_refptr<BitmapCursorOzone> > DefaultCursorMap;
  DefaultCursorMap default_cursors_;

  DISALLOW_COPY_AND_ASSIGN(BitmapCursorFactoryOzone);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_OZONE_BITMAP_CURSOR_FACTORY_OZONE_H_
