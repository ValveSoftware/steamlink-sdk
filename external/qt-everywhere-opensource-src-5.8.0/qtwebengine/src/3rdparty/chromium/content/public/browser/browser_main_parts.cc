// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_main_parts.h"

namespace content {

int BrowserMainParts::PreCreateThreads() {
  return 0;
}

bool BrowserMainParts::MainMessageLoopRun(int* result_code) {
  return false;
}

}  // namespace content
