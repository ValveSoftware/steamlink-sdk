// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/timer/hi_res_timer_manager.h"
#include "content/child/child_process.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_init.h"
#include "content/worker/worker_thread.h"

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"
#endif

#if defined(OS_MACOSX)
#include "content/common/sandbox_mac.h"
#endif

namespace content {

// Mainline routine for running as the worker process.
int WorkerMain(const MainFunctionParams& parameters) {
  // The main message loop of the worker process.
  base::MessageLoop main_message_loop;
  base::PlatformThread::SetName("CrWorkerMain");
  base::debug::TraceLog::GetInstance()->SetProcessName("Shared Web Worker");

#if defined(OS_WIN)
  sandbox::TargetServices* target_services =
      parameters.sandbox_info->target_services;
  if (!target_services)
    return false;

  // Cause advapi32 to load before the sandbox is turned on.
  unsigned int dummy_rand;
  rand_s(&dummy_rand);
  // Warm up language subsystems before the sandbox is turned on.
  ::GetUserDefaultLangID();
  ::GetUserDefaultLCID();

  target_services->LowerToken();
#elif defined(OS_MACOSX)
  // Sandbox should already be activated at this point.
  CHECK(Sandbox::SandboxIsCurrentlyActive());
#elif defined(OS_LINUX)
  // On Linux, the sandbox must be initialized early, before any thread is
  // created.
  LinuxSandbox::InitializeSandbox();
#endif

  ChildProcess worker_process;
  worker_process.set_main_thread(new WorkerThread());

  base::HighResolutionTimerManager hi_res_timer_manager;

  const CommandLine& parsed_command_line = parameters.command_line;
  if (parsed_command_line.HasSwitch(switches::kWaitForDebugger)) {
    ChildProcess::WaitForDebugger("Worker");
  }

  // Load the accelerator table from the browser executable and tell the
  // message loop to use it when translating messages.
  base::MessageLoop::current()->Run();

  return 0;
}

}  // namespace content
