// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_OZONE_BITMAP_CURSOR_FACTORY_OZONE_H_
#define UI_BASE_CURSOR_OZONE_BITMAP_CURSOR_FACTORY_OZONE_H_

#include <map>

#include "base/macros.h"
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
  BitmapCursorOzone(const SkBitmap& bitmap, const gfx::Point& hotspot);
  BitmapCursorOzone(const std::vector<SkBitmap>& bitmaps,
                    const gfx::Point& hotspot,
                    int frame_delay_ms);

  const gfx::Point& hotspot();
  const SkBitmap& bitmap();

  // For animated cursors.
  const std::vector<SkBitmap>& bitmaps();
  int frame_delay_ms();

 private:
  friend class base::RefCounted<BitmapCursorOzone>;
  ~BitmapCursorOzone();

  std::vector<SkBitmap> bitmaps_;
  gfx::Point hotspot_;
  int frame_delay_ms_;

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
  ~BitmapCursorFactoryOzone() override;

  // Convert PlatformCursor to BitmapCursorOzone.
  static scoped_refptr<BitmapCursorOzone> GetBitmapCursor(
      PlatformCursor platform_cursor);

  // CursorFactoryOzone:
  PlatformCursor GetDefaultCursor(int type) override;
  PlatformCursor CreateImageCursor(const SkBitmap& bitmap,
                                   const gfx::Point& hotspot) override;
  PlatformCursor CreateAnimatedCursor(
      const std::vector<SkBitmap>& bitmaps,
      const gfx::Point& hotspot,
      int frame_delay_ms) override;
  void RefImageCursor(PlatformCursor cursor) override;
  void UnrefImageCursor(PlatformCursor cursor) override;

 private:
  // Get cached BitmapCursorOzone for a default cursor.
  scoped_refptr<BitmapCursorOzone> GetDefaultCursorInternal(int type);

  // Default cursors are cached & owned by the factory.
  typedef std::map<int, scoped_refptr<BitmapCursorOzone> > DefaultCursorMap;
  DefaultCursorMap default_cursors_;

  DISALLOW_COPY_AND_ASSIGN(BitmapCursorFactoryOzone);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_OZONE_BITMAP_CURSOR_FACTORY_OZONE_H_
