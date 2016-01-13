// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_ozone.h"

#include "ui/base/cursor/cursor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/ozone/public/cursor_factory_ozone.h"

namespace ui {

CursorLoaderOzone::CursorLoaderOzone() {}

CursorLoaderOzone::~CursorLoaderOzone() {}

void CursorLoaderOzone::LoadImageCursor(int id,
                                        int resource_id,
                                        const gfx::Point& hot) {
  const gfx::ImageSkia* image =
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  const gfx::ImageSkiaRep& image_rep =
      image->GetRepresentation(scale());
  SkBitmap bitmap = image_rep.sk_bitmap();
  cursors_[id] =
      CursorFactoryOzone::GetInstance()->CreateImageCursor(bitmap, hot);
}

void CursorLoaderOzone::LoadAnimatedCursor(int id,
                                           int resource_id,
                                           const gfx::Point& hot,
                                           int frame_delay_ms) {
  // TODO(dnicoara) Add support: crbug.com/343245
  NOTIMPLEMENTED();
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
