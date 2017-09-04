// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMockClipboard_h
#define WebMockClipboard_h

#include "public/platform/WebClipboard.h"
#include "public/platform/WebImage.h"

namespace blink {

// Provides convenience methods for retrieving data from the mock clipboard
// used in layout and unit tests.
class WebMockClipboard : public WebClipboard {
 public:
  virtual WebImage readRawImage(Buffer) { return WebImage(); }
};

}  // namespace blink

#endif
