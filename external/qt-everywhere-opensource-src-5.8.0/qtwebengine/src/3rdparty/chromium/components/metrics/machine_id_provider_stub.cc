// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/machine_id_provider.h"

namespace metrics {

MachineIdProvider::MachineIdProvider() {
}

MachineIdProvider::~MachineIdProvider() {
}

// static
MachineIdProvider* MachineIdProvider::CreateInstance() {
  return NULL;
}

std::string MachineIdProvider::GetMachineId() {
  return std::string();
}

}  //  namespace metrics
