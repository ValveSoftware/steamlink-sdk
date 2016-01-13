// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include <windows.h>

#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "ui/gfx/icon_util.h"

namespace content {

const ui::PlatformCursor WebCursor::GetPlatformCursor() {
  if (!IsCustom())
    return LoadCursor(NULL, IDC_ARROW);

  if (custom_cursor_)
    return custom_cursor_;

  custom_cursor_ =
      IconUtil::CreateCursorFromDIB(
          custom_size_,
          hotspot_,
          !custom_data_.empty() ? &custom_data_[0] : NULL,
          custom_data_.size());
  return custom_cursor_;
}

void WebCursor::SetDisplayInfo(const gfx::Display& display) {
  // TODO(winguru): Add support for scaling the cursor.
}

void WebCursor::InitPlatformData() {
  custom_cursor_ = NULL;
}

bool WebCursor::SerializePlatformData(Pickle* pickle) const {
  return true;
}

bool WebCursor::DeserializePlatformData(PickleIterator* iter) {
  return true;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  if (custom_cursor_) {
    DestroyIcon(custom_cursor_);
    custom_cursor_ = NULL;
  }
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
}

}  // namespace content
