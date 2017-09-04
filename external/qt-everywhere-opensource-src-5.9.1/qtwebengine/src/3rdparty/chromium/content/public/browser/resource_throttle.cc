// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_throttle.h"

namespace content {

bool ResourceThrottle::MustProcessResponseBeforeReadingBody() {
  return false;
}

}  // namespace content
