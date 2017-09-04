// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebMemoryCoordinator.h"

#include "platform/MemoryCoordinator.h"

namespace blink {

void WebMemoryCoordinator::onMemoryPressure(
    WebMemoryPressureLevel pressureLevel) {
  MemoryCoordinator::instance().onMemoryPressure(pressureLevel);
}

void WebMemoryCoordinator::onMemoryStateChange(MemoryState state) {
  MemoryCoordinator::instance().onMemoryStateChange(state);
}

}  // namespace blink
