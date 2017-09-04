// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/runner/host/in_process_native_runner.h"

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
#include "services/service_manager/runner/host/native_library_runner.h"
#include "services/service_manager/runner/host/out_of_process_native_runner.h"
#include "services/service_manager/runner/init.h"

namespace service_manager {

InProcessNativeRunner::InProcessNativeRunner() : library_(nullptr) {}

InProcessNativeRunner::~InProcessNativeRunner() {
  // It is important to let the thread exit before unloading the DSO (when
  // library_ is destructed), because the library may have registered
  // thread-local data and destructors to run on thread termination.
  if (thread_) {
    DCHECK(thread_->HasBeenStarted());
    DCHECK(!thread_->HasBeenJoined());
    thread_->Join();
  }
}

mojom::ServicePtr InProcessNativeRunner::Start(
    const base::FilePath& library_path,
    const Identity& target,
    bool start_sandboxed,
    const base::Callback<void(base::ProcessId)>& pid_available_callback,
    const base::Closure& service_completed_callback) {
  library_path_ = library_path;

  DCHECK(!request_.is_pending());
  mojom::ServicePtr client;
  request_ = GetProxy(&client);

  DCHECK(service_completed_callback_runner_.is_null());
  service_completed_callback_runner_ = base::Bind(
      &base::TaskRunner::PostTask, base::ThreadTaskRunnerHandle::Get(),
      FROM_HERE, service_completed_callback);

  DCHECK(!thread_);
  std::string thread_name = "Service Thread";
#if defined(OS_WIN)
  thread_name = base::WideToUTF8(library_path_.BaseName().value());
#endif
  thread_.reset(new base::DelegateSimpleThread(this, thread_name));
  thread_->Start();
  pid_available_callback.Run(base::Process::Current().Pid());

  return client;
}

void InProcessNativeRunner::Run() {
  DVLOG(2) << "Loading/running Service in process from library: "
           << library_path_.value()
           << " thread id=" << base::PlatformThread::CurrentId();

  // TODO(vtl): ScopedNativeLibrary doesn't have a .get() method!
  base::NativeLibrary library = LoadNativeLibrary(library_path_);
  library_.Reset(library);
  // This hangs on Windows in the component build, so skip it since it's
  // unnecessary.
#if !(defined(COMPONENT_BUILD) && defined(OS_WIN))
  CallLibraryEarlyInitialization(library);
#endif
  RunServiceInNativeLibrary(library, std::move(request_));
  service_completed_callback_runner_.Run();
  service_completed_callback_runner_.Reset();
}

std::unique_ptr<NativeRunner> InProcessNativeRunnerFactory::Create(
    const base::FilePath& library_path) {
  // Executables are always run in a new process.
  if (!library_path.MatchesExtension(FILE_PATH_LITERAL(".library"))) {
    return base::MakeUnique<OutOfProcessNativeRunner>(launch_process_runner_,
                                                      nullptr);
  }
  return base::WrapUnique(new InProcessNativeRunner);
}

}  // namespace service_manager
