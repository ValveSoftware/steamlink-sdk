// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursors_aura.h"

namespace ui {

namespace {

BitmapCursorOzone* ToBitmapCursorOzone(PlatformCursor cursor) {
  return static_cast<BitmapCursorOzone*>(cursor);
}

PlatformCursor ToPlatformCursor(BitmapCursorOzone* cursor) {
  return static_cast<PlatformCursor>(cursor);
}

}  // namespace

BitmapCursorFactoryOzone::BitmapCursorFactoryOzone() {}

BitmapCursorFactoryOzone::~BitmapCursorFactoryOzone() {}

PlatformCursor BitmapCursorFactoryOzone::GetDefaultCursor(int type) {
  if (type == kCursorNone)
    return NULL;  // NULL is used for hidden cursor.

  if (!default_cursors_.count(type)) {
    // Create new owned image cursor from default aura bitmap for this type.
    SkBitmap bitmap;
    gfx::Point hotspot;
    CHECK(GetCursorBitmap(type, &bitmap, &hotspot));
    default_cursors_[type] = new BitmapCursorOzone(bitmap, hotspot);
  }

  // Returned owned default cursor for this type.
  return default_cursors_[type];
}

PlatformCursor BitmapCursorFactoryOzone::CreateImageCursor(
    const SkBitmap& bitmap,
    const gfx::Point& hotspot) {
  BitmapCursorOzone* cursor = new BitmapCursorOzone(bitmap, hotspot);
  cursor->AddRef();  // Balanced by UnrefImageCursor.
  return ToPlatformCursor(cursor);
}

void BitmapCursorFactoryOzone::RefImageCursor(PlatformCursor cursor) {
  ToBitmapCursorOzone(cursor)->AddRef();
}

void BitmapCursorFactoryOzone::UnrefImageCursor(PlatformCursor cursor) {
  ToBitmapCursorOzone(cursor)->Release();
}

void BitmapCursorFactoryOzone::SetCursor(gfx::AcceleratedWidget widget,
                                         PlatformCursor platform_cursor) {
  BitmapCursorOzone* cursor = ToBitmapCursorOzone(platform_cursor);
  SetBitmapCursor(widget, make_scoped_refptr(cursor));
}

void BitmapCursorFactoryOzone::SetBitmapCursor(
    gfx::AcceleratedWidget widget,
    scoped_refptr<BitmapCursorOzone>) {
  NOTIMPLEMENTED();
}

}  // namespace ui
