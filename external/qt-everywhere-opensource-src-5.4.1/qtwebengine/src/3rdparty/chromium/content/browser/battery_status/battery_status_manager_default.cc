// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/battery_status/battery_status_manager.h"

#include "base/logging.h"

namespace content {

BatteryStatusManager::BatteryStatusManager(
    const BatteryStatusService::BatteryUpdateCallback& callback)
    : callback_(callback) {
}

BatteryStatusManager::BatteryStatusManager() {
}

BatteryStatusManager::~BatteryStatusManager() {
}

bool BatteryStatusManager::StartListeningBatteryChange() {
  NOTIMPLEMENTED();
  return false;
}

void BatteryStatusManager::StopListeningBatteryChange() {
  NOTIMPLEMENTED();
}

}  // namespace content
