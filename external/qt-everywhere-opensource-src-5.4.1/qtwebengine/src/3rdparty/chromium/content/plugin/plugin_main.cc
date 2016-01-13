// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/timer/hi_res_timer_manager.h"
#include "content/child/child_process.h"
#include "content/common/content_constants_internal.h"
#include "content/plugin/plugin_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "content/public/common/injection_test_win.h"
#include "sandbox/win/src/sandbox.h"
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/posix/global_descriptors.h"
#include "ipc/ipc_descriptors.h"
#endif

namespace content {

#if defined(OS_MACOSX)
// Removes our Carbon library interposing from the environment so that it
// doesn't carry into any processes that plugins might start.
void TrimInterposeEnvironment();

// Initializes the global Cocoa application object.
void InitializeChromeApplication();
#endif

// main() routine for running as the plugin process.
int PluginMain(const MainFunctionParams& parameters) {
  // The main thread of the plugin services UI.
#if defined(OS_MACOSX)
#if !defined(__LP64__)
  TrimInterposeEnvironment();
#endif
  InitializeChromeApplication();
#endif
  base::MessageLoopForUI main_message_loop;
  base::PlatformThread::SetName("CrPluginMain");
  base::debug::TraceLog::GetInstance()->SetProcessName("Plugin Process");
  base::debug::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventPluginProcessSortIndex);

  const CommandLine& parsed_command_line = parameters.command_line;

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer com_initializer;
#endif

  if (parsed_command_line.HasSwitch(switches::kPluginStartupDialog)) {
    ChildProcess::WaitForDebugger("Plugin");
  }

  {
    ChildProcess plugin_process;
    plugin_process.set_main_thread(new PluginThread());
    base::HighResolutionTimerManager hi_res_timer_manager;
    base::MessageLoop::current()->Run();
  }

  return 0;
}

}  // namespace content
