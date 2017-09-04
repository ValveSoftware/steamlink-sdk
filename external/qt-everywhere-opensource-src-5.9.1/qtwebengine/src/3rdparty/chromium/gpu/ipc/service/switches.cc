// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/switches.h"

namespace switches {

// Disable the thread that crashes the GPU process if it stops responding to
// messages.
const char kDisableGpuWatchdog[] = "disable-gpu-watchdog";

// Starts the GPU sandbox before creating a GL context.
const char kGpuSandboxStartEarly[] = "gpu-sandbox-start-early";

}  // namespace switches
