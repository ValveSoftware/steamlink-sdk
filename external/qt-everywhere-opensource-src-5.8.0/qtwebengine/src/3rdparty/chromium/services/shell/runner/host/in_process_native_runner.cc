// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/runner/host/in_process_native_runner.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/shell/runner/host/native_application_support.h"
#include "services/shell/runner/host/out_of_process_native_runner.h"
#include "services/shell/runner/init.h"

namespace shell {

InProcessNativeRunner::InProcessNativeRunner() : app_library_(nullptr) {}

InProcessNativeRunner::~InProcessNativeRunner() {
  // It is important to let the thread exit before unloading the DSO (when
  // app_library_ is destructed), because the library may have registered
  // thread-local data and destructors to run on thread termination.
  if (thread_) {
    DCHECK(thread_->HasBeenStarted());
    DCHECK(!thread_->HasBeenJoined());
    thread_->Join();
  }
}

mojom::ShellClientPtr InProcessNativeRunner::Start(
    const base::FilePath& app_path,
    const Identity& target,
    bool start_sandboxed,
    const base::Callback<void(base::ProcessId)>& pid_available_callback,
    const base::Closure& app_completed_callback) {
  app_path_ = app_path;

  DCHECK(!request_.is_pending());
  mojom::ShellClientPtr client;
  request_ = GetProxy(&client);

  DCHECK(app_completed_callback_runner_.is_null());
  app_completed_callback_runner_ = base::Bind(
      &base::TaskRunner::PostTask, base::ThreadTaskRunnerHandle::Get(),
      FROM_HERE, app_completed_callback);

  DCHECK(!thread_);
  std::string thread_name = "mojo:app_thread";
#if defined(OS_WIN)
  thread_name = base::WideToUTF8(app_path_.BaseName().value());
#endif
  thread_.reset(new base::DelegateSimpleThread(this, thread_name));
  thread_->Start();
  pid_available_callback.Run(base::Process::Current().Pid());

  return client;
}

void InProcessNativeRunner::Run() {
  DVLOG(2) << "Loading/running Mojo app in process from library: "
           << app_path_.value()
           << " thread id=" << base::PlatformThread::CurrentId();

  // TODO(vtl): ScopedNativeLibrary doesn't have a .get() method!
  base::NativeLibrary app_library = LoadNativeApplication(app_path_);
  app_library_.Reset(app_library);
  // This hangs on Windows in the component build, so skip it since it's
  // unnecessary.
#if !(defined(COMPONENT_BUILD) && defined(OS_WIN))
  CallLibraryEarlyInitialization(app_library);
#endif
  RunNativeApplication(app_library, std::move(request_));
  app_completed_callback_runner_.Run();
  app_completed_callback_runner_.Reset();
}

std::unique_ptr<NativeRunner> InProcessNativeRunnerFactory::Create(
    const base::FilePath& app_path) {
  // Non-Mojo apps are always run in a new process.
  if (!app_path.MatchesExtension(FILE_PATH_LITERAL(".mojo"))) {
    return base::WrapUnique(
        new OutOfProcessNativeRunner(launch_process_runner_, nullptr));
  }
  return base::WrapUnique(new InProcessNativeRunner);
}

}  // namespace shell
