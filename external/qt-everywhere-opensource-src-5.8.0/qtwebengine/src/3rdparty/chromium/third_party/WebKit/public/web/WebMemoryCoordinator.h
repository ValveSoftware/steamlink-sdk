// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMemoryCoordinator_h
#define WebMemoryCoordinator_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebMemoryPressureLevel.h"

namespace blink {

class WebMemoryCoordinator {
public:
    // Called when a memory pressure notification is received.
    BLINK_EXPORT static void onMemoryPressure(WebMemoryPressureLevel);
};

} // namespace blink

#endif
