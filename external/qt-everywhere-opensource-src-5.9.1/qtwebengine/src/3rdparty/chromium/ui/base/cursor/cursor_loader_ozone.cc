// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_ozone.h"

#include <vector>

#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_util.h"
#include "ui/ozone/public/cursor_factory_ozone.h"

namespace ui {

CursorLoaderOzone::CursorLoaderOzone() {}

CursorLoaderOzone::~CursorLoaderOzone() {}

void CursorLoaderOzone::LoadImageCursor(int id,
                                        int resource_id,
                                        const gfx::Point& hot) {
  SkBitmap bitmap;
  gfx::Point hotspot = hot;

  GetImageCursorBitmap(resource_id, scale(), rotation(), &hotspot, &bitmap);

  cursors_[id] =
      CursorFactoryOzone::GetInstance()->CreateImageCursor(bitmap, hotspot);
}

void CursorLoaderOzone::LoadAnimatedCursor(int id,
                                           int resource_id,
                                           const gfx::Point& hot,
                                           int frame_delay_ms) {
  std::vector<SkBitmap> bitmaps;
  gfx::Point hotspot = hot;

  GetAnimatedCursorBitmaps(
      resource_id, scale(), rotation(), &hotspot, &bitmaps);

  cursors_[id] = CursorFactoryOzone::GetInstance()->CreateAnimatedCursor(
      bitmaps, hotspot, frame_delay_ms);
}

void CursorLoaderOzone::UnloadAll() {
  for (ImageCursorMap::const_iterator it = cursors_.begin();
       it != cursors_.end();
       ++it)
    CursorFactoryOzone::GetInstance()->UnrefImageCursor(it->second);
  cursors_.clear();
}

void CursorLoaderOzone::SetPlatformCursor(gfx::NativeCursor* cursor) {
  int native_type = cursor->native_type();
  PlatformCursor platform;

  if (cursors_.count(native_type)) {
    // An image cursor is loaded for this type.
    platform = cursors_[native_type];
  } else if (native_type == kCursorCustom) {
    // The platform cursor was already set via WebCursor::GetPlatformCursor.
    platform = cursor->platform();
  } else {
    // Use default cursor of this type.
    platform = CursorFactoryOzone::GetInstance()->GetDefaultCursor(native_type);
  }

  cursor->SetPlatformCursor(platform);
}

CursorLoader* CursorLoader::Create() {
  return new CursorLoaderOzone();
}

}  // namespace ui
