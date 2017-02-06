// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/runner/host/child_process.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/core.h"
#include "services/shell/runner/common/switches.h"
#include "services/shell/runner/host/child_process_base.h"
#include "services/shell/runner/host/native_application_support.h"
#include "services/shell/runner/init.h"

namespace shell {

namespace {

void RunNativeLibrary(base::NativeLibrary app_library,
                      mojom::ShellClientRequest shell_client_request) {
  if (!RunNativeApplication(app_library, std::move(shell_client_request))) {
    LOG(ERROR) << "Failure to RunNativeApplication()";
  }
}

}  // namespace

int ChildProcessMain() {
  DVLOG(2) << "ChildProcessMain()";
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  base::NativeLibrary app_library = 0;
  // Load the application library before we engage the sandbox.
  base::FilePath app_library_path =
      command_line.GetSwitchValuePath(switches::kChildProcess);
  if (!app_library_path.empty())
    app_library = LoadNativeApplication(app_library_path);
  base::i18n::InitializeICU();
  if (app_library)
    CallLibraryEarlyInitialization(app_library);

  ChildProcessMainWithCallback(base::Bind(&RunNativeLibrary, app_library));

  return 0;
}

}  // namespace shell
