// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/device_id.h"
#include "base/logging.h"

MachineIdStatus GetDeterministicMachineSpecificId(std::string* machine_id) {
  DCHECK(machine_id);
  return MachineIdStatus::NOT_IMPLEMENTED;
}
