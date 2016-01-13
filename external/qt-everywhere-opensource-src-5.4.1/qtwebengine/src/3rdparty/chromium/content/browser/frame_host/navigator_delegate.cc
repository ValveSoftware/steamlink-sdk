// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator_delegate.h"

namespace content {

bool NavigatorDelegate::CanOverscrollContent() const {
  return false;
}

bool NavigatorDelegate::ShouldPreserveAbortedURLs() {
  return false;
}

}  // namespace content
