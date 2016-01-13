// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/out_of_process_dynamic_service_runner.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"

namespace mojo {
namespace shell {

OutOfProcessDynamicServiceRunner::OutOfProcessDynamicServiceRunner(
    Context* context)
    : context_(context),
      keep_alive_(context) {
}

OutOfProcessDynamicServiceRunner::~OutOfProcessDynamicServiceRunner() {
  if (app_child_process_host_) {
    // TODO(vtl): Race condition: If |AppChildProcessHost::DidStart()| hasn't
    // been called yet, we shouldn't call |Join()| here. (Until |DidStart()|, we
    // may not have a child process to wait on.) Probably we should fix
    // |Join()|.
    app_child_process_host_->Join();
  }
}

void OutOfProcessDynamicServiceRunner::Start(
    const base::FilePath& app_path,
    ScopedMessagePipeHandle service_handle,
    const base::Closure& app_completed_callback) {
  app_path_ = app_path;

  DCHECK(!service_handle_.is_valid());
  service_handle_ = service_handle.Pass();

  DCHECK(app_completed_callback_.is_null());
  app_completed_callback_ = app_completed_callback;

  app_child_process_host_.reset(new AppChildProcessHost(context_, this));
  app_child_process_host_->Start();

  // TODO(vtl): |app_path.AsUTF8Unsafe()| is unsafe.
  app_child_process_host_->controller()->StartApp(
      app_path.AsUTF8Unsafe(),
      ScopedMessagePipeHandle(MessagePipeHandle(
          service_handle.release().value())));
}

void OutOfProcessDynamicServiceRunner::AppCompleted(int32_t result) {
  DVLOG(2) << "OutOfProcessDynamicServiceRunner::AppCompleted(" << result
           << ")";

  app_completed_callback_.Run();
  app_completed_callback_.Reset();
  app_child_process_host_.reset();
}

}  // namespace shell
}  // namespace mojo
