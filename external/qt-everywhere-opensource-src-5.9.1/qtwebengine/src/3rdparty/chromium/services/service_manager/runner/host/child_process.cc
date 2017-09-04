// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/runner/host/child_process.h"

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
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/runner/host/child_process_base.h"
#include "services/service_manager/runner/host/native_library_runner.h"
#include "services/service_manager/runner/init.h"

namespace service_manager {

namespace {

void RunNativeLibrary(base::NativeLibrary library,
                      mojom::ServiceRequest service_request) {
  if (!RunServiceInNativeLibrary(library, std::move(service_request))) {
    LOG(ERROR) << "Failure to RunServiceInNativeLibrary()";
  }
}

}  // namespace

int ChildProcessMain() {
  DVLOG(2) << "ChildProcessMain()";
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  base::NativeLibrary library = 0;
  // Load the application library before we engage the sandbox.
  base::FilePath library_path =
      command_line.GetSwitchValuePath(switches::kChildProcess);
  if (!library_path.empty())
    library = LoadNativeLibrary(library_path);
  base::i18n::InitializeICU();
  if (library)
    CallLibraryEarlyInitialization(library);

  ChildProcessMainWithCallback(base::Bind(&RunNativeLibrary, library));

  return 0;
}

}  // namespace service_manager
