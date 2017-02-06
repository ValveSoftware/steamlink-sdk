// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dummy implementation of reboot.h

#include "base/logging.h"
#include "chromecast/system/reboot/reboot.h"

namespace chromecast {

RebootType GetLastRebootType() {
  return RebootType::UNKNOWN;
}

base::Time GetLastRebootTime() {
  return base::Time();
}

bool DoReboot(RebootType type) {
  LOG(ERROR) << "Dummy DoReboot called, no action taken";
  return false;
}

bool DoRebootApi(RebootCommand command) {
  LOG(ERROR) << "Dummy DoRebootApi called, no action taken";
  return false;
}

}  // namespace chromecast
