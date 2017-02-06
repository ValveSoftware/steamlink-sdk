// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_cursor_factory_ozone.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

namespace {

X11CursorOzone* ToX11CursorOzone(PlatformCursor cursor) {
  return static_cast<X11CursorOzone*>(cursor);
}

PlatformCursor ToPlatformCursor(X11CursorOzone* cursor) {
  return static_cast<PlatformCursor>(cursor);
}

// Gets default aura cursor bitmap/hotspot and creates a X11CursorOzone with it.
scoped_refptr<X11CursorOzone> CreateAuraX11Cursor(int type) {
  SkBitmap bitmap;
  gfx::Point hotspot;
  if (GetCursorBitmap(type, &bitmap, &hotspot)) {
    return new X11CursorOzone(bitmap, hotspot);
  }
  return nullptr;
}

}  // namespace

X11CursorFactoryOzone::X11CursorFactoryOzone()
    : invisible_cursor_(X11CursorOzone::CreateInvisible()) {}

X11CursorFactoryOzone::~X11CursorFactoryOzone() {}

PlatformCursor X11CursorFactoryOzone::GetDefaultCursor(int type) {
  return ToPlatformCursor(GetDefaultCursorInternal(type).get());
}

PlatformCursor X11CursorFactoryOzone::CreateImageCursor(
    const SkBitmap& bitmap,
    const gfx::Point& hotspot) {
  // There is a problem with custom cursors that have no custom data. The
  // resulting SkBitmap is empty and X crashes when creating a zero size cursor
  // image. Return invisible cursor here instead.
  if (bitmap.drawsNothing()) {
    return ToPlatformCursor(invisible_cursor_.get());
  }

  X11CursorOzone* cursor = new X11CursorOzone(bitmap, hotspot);
  cursor->AddRef();
  return ToPlatformCursor(cursor);
}

PlatformCursor X11CursorFactoryOzone::CreateAnimatedCursor(
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    int frame_delay_ms) {
  X11CursorOzone* cursor = new X11CursorOzone(bitmaps, hotspot, frame_delay_ms);
  cursor->AddRef();
  return ToPlatformCursor(cursor);
}

void X11CursorFactoryOzone::RefImageCursor(PlatformCursor cursor) {
  ToX11CursorOzone(cursor)->AddRef();
}

void X11CursorFactoryOzone::UnrefImageCursor(PlatformCursor cursor) {
  ToX11CursorOzone(cursor)->Release();
}

scoped_refptr<X11CursorOzone> X11CursorFactoryOzone::GetDefaultCursorInternal(
    int type) {
  if (type == kCursorNone)
    return invisible_cursor_;

  // TODO(kylechar): Use predefined X cursors here instead.
  if (!default_cursors_.count(type)) {
    // Loads the default aura cursor bitmap for cursor type. Falls back on
    // pointer cursor then invisible cursor if this fails.
    scoped_refptr<X11CursorOzone> cursor = CreateAuraX11Cursor(type);
    if (!cursor.get()) {
      if (type != kCursorPointer) {
        cursor = GetDefaultCursorInternal(kCursorPointer);
      } else {
        NOTREACHED() << "Failed to load default cursor bitmap";
      }
    }
    default_cursors_[type] = cursor;
  }

  // Returns owned default cursor for this type.
  return default_cursors_[type];
}

}  // namespace ui
