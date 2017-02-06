// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_RUNNER_HOST_CHILD_PROCESS_HOST_H_
#define SERVICES_SHELL_RUNNER_HOST_CHILD_PROCESS_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/shell_client_factory.mojom.h"
#include "services/shell/runner/host/child_process_host.h"

namespace base {
class TaskRunner;
}

namespace shell {

class Identity;
class NativeRunnerDelegate;

// This class represents a "child process host". Handles launching and
// connecting a platform-specific "pipe" to the child, and supports joining the
// child process. Currently runs a single app (loaded from the file system).
//
// This class is not thread-safe. It should be created/used/destroyed on a
// single thread.
//
// Note: Does not currently work on Windows before Vista.
// Note: After |Start()|, |StartApp| must be called and this object must
// remained alive until the |on_app_complete| callback is called.
class ChildProcessHost {
 public:
  using ProcessReadyCallback = base::Callback<void(base::ProcessId)>;

  // |name| is just for debugging ease. We will spawn off a process so that it
  // can be sandboxed if |start_sandboxed| is true. |app_path| is a path to the
  // mojo application we wish to start.
  ChildProcessHost(base::TaskRunner* launch_process_runner,
                   NativeRunnerDelegate* delegate,
                   bool start_sandboxed,
                   const Identity& target,
                   const base::FilePath& app_path);
  virtual ~ChildProcessHost();

  // |Start()|s the child process; calls |DidStart()| (on the thread on which
  // |Start()| was called) when the child has been started (or failed to start).
  mojom::ShellClientPtr Start(const Identity& target,
                              const ProcessReadyCallback& callback,
                              const base::Closure& quit_closure);

  // Waits for the child process to terminate.
  void Join();

 protected:
  void DidStart(const ProcessReadyCallback& callback);

 private:
  void DoLaunch(std::unique_ptr<base::CommandLine> child_command_line);

  scoped_refptr<base::TaskRunner> launch_process_runner_;
  NativeRunnerDelegate* delegate_ = nullptr;
  bool start_sandboxed_ = false;
  Identity target_;
  const base::FilePath app_path_;
  base::Process child_process_;

  // Used to initialize the Mojo IPC channel between parent and child.
  std::unique_ptr<mojo::edk::PlatformChannelPair> mojo_ipc_channel_;
  mojo::edk::HandlePassingInformation handle_passing_info_;
  const std::string child_token_;

  // Since Start() calls a method on another thread, we use an event to block
  // the main thread if it tries to destruct |this| while launching the process.
  base::WaitableEvent start_child_process_event_;

  base::WeakPtrFactory<ChildProcessHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessHost);
};

}  // namespace shell

#endif  // SERVICES_SHELL_RUNNER_HOST_CHILD_PROCESS_HOST_H_
