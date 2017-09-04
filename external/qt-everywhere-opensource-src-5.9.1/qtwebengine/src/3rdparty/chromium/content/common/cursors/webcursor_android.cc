// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"

namespace content {

gfx::NativeCursor WebCursor::GetNativeCursor() {
  return gfx::kNullCursor;
}

#if defined(USE_AURA)
// In the future when we want to support cursors of various kinds in Aura on
// Android, we should switch to using webcursor_aura rather than add an
// implementation here.
void WebCursor::SetDisplayInfo(const display::Display& display) {
}
#endif

void WebCursor::InitPlatformData() {
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
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
}

}  // namespace content
