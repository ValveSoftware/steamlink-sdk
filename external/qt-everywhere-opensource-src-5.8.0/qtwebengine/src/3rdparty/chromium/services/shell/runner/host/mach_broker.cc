// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/runner/host/mach_broker.h"

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace shell {

namespace {
const char kBootstrapPortName[] = "mojo_shell";
}

// static
void MachBroker::SendTaskPortToParent() {
  bool result = base::MachPortBroker::ChildSendTaskPortToParent(
      kBootstrapPortName);
  DCHECK(result);
}

// static
MachBroker* MachBroker::GetInstance() {
  return base::Singleton<MachBroker>::get();
}

MachBroker::MachBroker() : broker_(kBootstrapPortName) {
  bool result = broker_.Init();
  DCHECK(result);
}

MachBroker::~MachBroker() {}

void MachBroker::ExpectPid(base::ProcessHandle pid) {
  broker_.AddPlaceholderForPid(pid);
}

void MachBroker::RemovePid(base::ProcessHandle pid) {
  broker_.InvalidatePid(pid);
}

}  // namespace shell
