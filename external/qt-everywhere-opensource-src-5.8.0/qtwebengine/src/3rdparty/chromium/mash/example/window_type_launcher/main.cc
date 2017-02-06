// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/i18n/icu_util.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mash/example/window_type_launcher/window_type_launcher.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"
#include "services/shell/runner/common/client_util.h"
#include "services/shell/runner/init.h"

namespace {

class ProcessDelegate : public mojo::edk::ProcessDelegate {
 public:
  ProcessDelegate() {}
  ~ProcessDelegate() override {}

 private:
  void OnShutdownComplete() override {}

  DISALLOW_COPY_AND_ASSIGN(ProcessDelegate);
};

}

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  shell::InitializeLogging();
  shell::WaitForDebuggerIfNecessary();

#if !defined(OFFICIAL_BUILD)
  base::debug::EnableInProcessStackDumping();
#if defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif
#endif

  {
    mojo::edk::Init();

    ProcessDelegate process_delegate;
    base::Thread io_thread("io_thread");
    base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread.StartWithOptions(io_thread_options));

    mojo::edk::InitIPCSupport(&process_delegate, io_thread.task_runner().get());
    mojo::edk::SetParentPipeHandleFromCommandLine();

    base::i18n::InitializeICU();

    base::MessageLoop loop;
    WindowTypeLauncher delegate;
    shell::ShellConnection impl(&delegate,
                                shell::GetShellClientRequestFromCommandLine());
    loop.Run();

    mojo::edk::ShutdownIPCSupport();
  }

  return 0;
}
