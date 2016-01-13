// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_OUT_OF_PROCESS_DYNAMIC_SERVICE_RUNNER_H_
#define MOJO_SHELL_OUT_OF_PROCESS_DYNAMIC_SERVICE_RUNNER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "mojo/shell/app_child_process.mojom.h"
#include "mojo/shell/app_child_process_host.h"
#include "mojo/shell/dynamic_service_runner.h"
#include "mojo/shell/keep_alive.h"

namespace mojo {
namespace shell {

// An implementation of |DynamicServiceRunner| that loads/runs the given app
// (from the file system) in a separate process (of its own).
class OutOfProcessDynamicServiceRunner
    : public DynamicServiceRunner,
      public AppChildControllerClient {
 public:
  explicit OutOfProcessDynamicServiceRunner(Context* context);
  virtual ~OutOfProcessDynamicServiceRunner();

  // |DynamicServiceRunner| method:
  virtual void Start(const base::FilePath& app_path,
                     ScopedMessagePipeHandle service_handle,
                     const base::Closure& app_completed_callback) OVERRIDE;

 private:
  // |AppChildControllerClient| method:
  virtual void AppCompleted(int32_t result) OVERRIDE;

  Context* const context_;

  KeepAlive keep_alive_;
  base::FilePath app_path_;
  ScopedMessagePipeHandle service_handle_;
  base::Closure app_completed_callback_;

  scoped_ptr<AppChildProcessHost> app_child_process_host_;

  DISALLOW_COPY_AND_ASSIGN(OutOfProcessDynamicServiceRunner);
};

typedef DynamicServiceRunnerFactoryImpl<OutOfProcessDynamicServiceRunner>
    OutOfProcessDynamicServiceRunnerFactory;

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_OUT_OF_PROCESS_DYNAMIC_SERVICE_RUNNER_H_
