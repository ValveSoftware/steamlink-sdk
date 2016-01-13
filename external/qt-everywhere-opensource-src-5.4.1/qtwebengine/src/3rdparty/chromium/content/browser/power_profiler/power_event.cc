// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_profiler/power_event.h"

namespace content {

const char* kPowerTypeNames[] = {
  "SoC_Package",
  "Device"
};

COMPILE_ASSERT(arraysize(kPowerTypeNames) == PowerEvent::ID_COUNT,
               kPowerTypeNames_incorrect_size);

}  // namespace content
