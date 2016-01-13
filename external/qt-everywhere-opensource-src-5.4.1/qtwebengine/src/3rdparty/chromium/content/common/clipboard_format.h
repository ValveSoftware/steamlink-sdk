// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CLIPBOARD_FORMAT_H_
#define CONTENT_COMMON_CLIPBOARD_FORMAT_H_

namespace content {

enum ClipboardFormat {
  CLIPBOARD_FORMAT_PLAINTEXT,
  CLIPBOARD_FORMAT_HTML,
  CLIPBOARD_FORMAT_SMART_PASTE,
  CLIPBOARD_FORMAT_BOOKMARK,
  CLIPBOARD_FORMAT_LAST = CLIPBOARD_FORMAT_BOOKMARK,
};

}  // namespace

#endif  // CONTENT_COMMON_CLIPBOARD_FORMAT_H_
