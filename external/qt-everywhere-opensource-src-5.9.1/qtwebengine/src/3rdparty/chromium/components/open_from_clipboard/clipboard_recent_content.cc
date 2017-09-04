// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content.h"

namespace {
ClipboardRecentContent* g_clipboard_recent_content = nullptr;
}

ClipboardRecentContent::ClipboardRecentContent() {}

ClipboardRecentContent::~ClipboardRecentContent() {}

// static
ClipboardRecentContent* ClipboardRecentContent::GetInstance() {
  return g_clipboard_recent_content;
}

// static
void ClipboardRecentContent::SetInstance(ClipboardRecentContent* instance) {
  g_clipboard_recent_content = instance;
}
