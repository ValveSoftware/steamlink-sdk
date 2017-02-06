// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/memory_handler.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/strings/stringprintf.h"
#include "content/browser/memory/memory_pressure_controller_impl.h"

namespace content {
namespace devtools {
namespace memory {

MemoryHandler::MemoryHandler() {}

MemoryHandler::~MemoryHandler() {}

MemoryHandler::Response MemoryHandler::SetPressureNotificationsSuppressed(
    bool suppressed) {
  content::MemoryPressureControllerImpl::GetInstance()
      ->SetPressureNotificationsSuppressedInAllProcesses(suppressed);
  return Response::OK();
}

MemoryHandler::Response MemoryHandler::SimulatePressureNotification(
    const std::string& level) {
  base::MemoryPressureListener::MemoryPressureLevel parsed_level;
  if (level == kPressureLevelModerate) {
    parsed_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  } else if (level == kPressureLevelCritical) {
    parsed_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  } else {
    return Response::InternalError(base::StringPrintf(
        "Invalid memory pressure level '%s'", level.c_str()));
  }

  MemoryPressureControllerImpl::GetInstance()
      ->SimulatePressureNotificationInAllProcesses(parsed_level);
  return Response::OK();
}

}  // namespace memory
}  // namespace devtools
}  // namespace content
