// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMemoryCoordinator_h
#define WebMemoryCoordinator_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebMemoryPressureLevel.h"
#include "public/platform/WebMemoryState.h"

namespace blink {

class WebMemoryCoordinator {
 public:
  // Called when a memory pressure notification is received.
  // TODO(bashi): Deprecating. Remove this when MemoryPressureListener is
  // gone.
  BLINK_PLATFORM_EXPORT static void onMemoryPressure(WebMemoryPressureLevel);

  BLINK_PLATFORM_EXPORT static void onMemoryStateChange(MemoryState);
};

}  // namespace blink

#endif
