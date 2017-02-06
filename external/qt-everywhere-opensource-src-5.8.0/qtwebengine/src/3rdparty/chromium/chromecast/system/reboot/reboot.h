// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SYSTEM_REBOOT_REBOOT_H_
#define CHROMECAST_SYSTEM_REBOOT_REBOOT_H_

#include <string>

#include "base/time/time.h"

namespace chromecast {

// Must be kept in sync with CastLogsProto::CastDeviceMutableInfo::RebootType
// located in //components/metrics/proto/cast_logs.proto
enum class RebootType {
  MIN = 0,

  UNKNOWN = 0,
  FORCED = 1,
  API = 2,
  NIGHTLY = 3,
  OTA = 4,
  WATCHDOG = 5,
  PROCESS_MANAGER = 6,
  CRASH_UPLOADER = 7,
  FDR = 8,

  MAX = FDR,
};

enum RebootCommand {
  REBOOT_CMD_NONE = 0,
  REBOOT_CMD_OTA = 1 << 0,
  REBOOT_CMD_FDR = 1 << 1,
  REBOOT_CMD_NOW = 1 << 2,
};

// Reports the last type of reboot.
RebootType GetLastRebootType();

// Reports the time of the last scheduled reboot.
base::Time GetLastRebootTime();

// Perform a reboot of type |type|.
bool DoReboot(RebootType type);

// TODO(ameyak): Remove from reboot interface
// Perform a reboot with command |command|.
bool DoRebootApi(RebootCommand command);

}  // namespace chromecast

#endif  // CHROMECAST_SYSTEM_REBOOT_REBOOT_H_
