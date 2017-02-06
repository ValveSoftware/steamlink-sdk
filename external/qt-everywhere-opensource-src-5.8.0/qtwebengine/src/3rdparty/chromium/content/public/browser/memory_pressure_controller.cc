// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/memory_pressure_controller.h"

#include "content/browser/memory/memory_pressure_controller_impl.h"

namespace content {

// static
void MemoryPressureController::SendPressureNotification(
    const BrowserChildProcessHost* child_process_host,
    base::MemoryPressureListener::MemoryPressureLevel level) {
  MemoryPressureControllerImpl::GetInstance()->SendPressureNotification(
      child_process_host, level);
}

// static
void MemoryPressureController::SendPressureNotification(
    const RenderProcessHost* render_process_host,
    base::MemoryPressureListener::MemoryPressureLevel level) {
  MemoryPressureControllerImpl::GetInstance()->SendPressureNotification(
      render_process_host, level);
}

}  // namespace content
