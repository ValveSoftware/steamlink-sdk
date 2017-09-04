// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib.h>

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader_x11.h"
#include "ui/base/x/x11_util.h"

namespace content {

ui::PlatformCursor WebCursor::GetPlatformCursor() {
  if (platform_cursor_)
    return platform_cursor_;

  SkBitmap bitmap;
  gfx::Point hotspot;
  CreateScaledBitmapAndHotspotFromCustomData(&bitmap, &hotspot);

  XcursorImage* image = ui::SkBitmapToXcursorImage(&bitmap, hotspot);
  platform_cursor_ = ui::CreateReffedCustomXCursor(image);
  return platform_cursor_;
}

void WebCursor::InitPlatformData() {
  platform_cursor_ = 0;
  device_scale_factor_ = 1.f;
}

bool WebCursor::SerializePlatformData(base::Pickle* pickle) const {
  return true;
}

bool WebCursor::DeserializePlatformData(base::PickleIterator* iter) {
  return true;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  if (platform_cursor_) {
    ui::UnrefCustomXCursor(platform_cursor_);
    platform_cursor_ = 0;
  }
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  if (platform_cursor_)
    ui::UnrefCustomXCursor(platform_cursor_);
  platform_cursor_ = other.platform_cursor_;
  if (platform_cursor_)
    ui::RefCustomXCursor(platform_cursor_);

  device_scale_factor_ = other.device_scale_factor_;
}

}  // namespace content
