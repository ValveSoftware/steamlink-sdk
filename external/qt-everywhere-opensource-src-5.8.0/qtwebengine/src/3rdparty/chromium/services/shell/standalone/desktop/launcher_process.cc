// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>

#include <iostream>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "services/shell/standalone/context.h"
#include "services/shell/switches.h"

namespace shell {

int LauncherProcessMain() {
#if !defined(OFFICIAL_BUILD)
  base::debug::EnableInProcessStackDumping();
#endif
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // http://crbug.com/546644
  command_line->AppendSwitch(switches::kNoSandbox);

  base::PlatformThread::SetName("mojo_runner");

  // We want the Context to outlive the MessageLoop so that pipes are all
  // gracefully closed / error-out before we try to shut the Context down.
  Context shell_context;
  {
    base::MessageLoop message_loop;
    CHECK(base::i18n::InitializeICU());
    shell_context.Init(nullptr);

    message_loop.task_runner()->PostTask(
        FROM_HERE, base::Bind(&Context::RunCommandLineApplication,
                              base::Unretained(&shell_context)));

    base::RunLoop().Run();

    // Must be called before |message_loop| is destroyed.
    shell_context.Shutdown();
  }

  return 0;
}

}  // namespace shell
