// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_RUNNER_HOST_IN_PROCESS_NATIVE_RUNNER_H_
#define SERVICES_SERVICE_MANAGER_RUNNER_HOST_IN_PROCESS_NATIVE_RUNNER_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_native_library.h"
#include "base/threading/simple_thread.h"
#include "services/service_manager/native_runner.h"

namespace base {
class TaskRunner;
}

namespace service_manager {

// An implementation of |NativeRunner| that loads/runs the given service (from
// the file system) on a separate thread (in the current process).
class InProcessNativeRunner : public NativeRunner,
                              public base::DelegateSimpleThread::Delegate {
 public:
  InProcessNativeRunner();
  ~InProcessNativeRunner() override;

  // NativeRunner:
  mojom::ServicePtr Start(
      const base::FilePath& library_path,
      const Identity& target,
      bool start_sandboxed,
      const base::Callback<void(base::ProcessId)>& pid_available_callback,
      const base::Closure& service_completed_callback) override;

 private:
  // |base::DelegateSimpleThread::Delegate| method:
  void Run() override;

  base::FilePath library_path_;
  mojom::ServiceRequest request_;
  base::Callback<bool(void)> service_completed_callback_runner_;

  base::ScopedNativeLibrary library_;
  std::unique_ptr<base::DelegateSimpleThread> thread_;

  DISALLOW_COPY_AND_ASSIGN(InProcessNativeRunner);
};

class InProcessNativeRunnerFactory : public NativeRunnerFactory {
 public:
  explicit InProcessNativeRunnerFactory(base::TaskRunner* launch_process_runner)
      : launch_process_runner_(launch_process_runner) {}
  ~InProcessNativeRunnerFactory() override {}

  std::unique_ptr<NativeRunner> Create(
      const base::FilePath& library_path) override;

 private:
  base::TaskRunner* const launch_process_runner_;

  DISALLOW_COPY_AND_ASSIGN(InProcessNativeRunnerFactory);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_RUNNER_HOST_IN_PROCESS_NATIVE_RUNNER_H_
