// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/background/background_service_manager_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/process/launch.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/runner/host/child_process.h"
#include "services/service_manager/runner/init.h"

namespace service_manager {
namespace {

int RunChildProcess() {
  base::AtExitManager at_exit;
  InitializeLogging();
  WaitForDebuggerIfNecessary();
#if !defined(OFFICIAL_BUILD) && defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif
  return ChildProcessMain();
}

}  // namespace
}  // namespace service_manager

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kChildProcess)) {
    return service_manager::RunChildProcess();
  }
  // Reset CommandLine as most likely main() is going to use CommandLine too
  // and expect to be able to initialize it.
  base::CommandLine::Reset();
  return MasterProcessMain(argc, argv);
}
