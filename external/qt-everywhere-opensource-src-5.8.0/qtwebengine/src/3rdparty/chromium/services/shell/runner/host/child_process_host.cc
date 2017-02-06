// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/runner/host/child_process_host.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/system/core.h"
#include "services/shell/native_runner_delegate.h"
#include "services/shell/runner/common/client_util.h"
#include "services/shell/runner/common/switches.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "sandbox/linux/services/namespace_sandbox.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_MACOSX)
#include "services/shell/runner/host/mach_broker.h"
#endif

namespace shell {

ChildProcessHost::ChildProcessHost(base::TaskRunner* launch_process_runner,
                                   NativeRunnerDelegate* delegate,
                                   bool start_sandboxed,
                                   const Identity& target,
                                   const base::FilePath& app_path)
    : launch_process_runner_(launch_process_runner),
      delegate_(delegate),
      start_sandboxed_(start_sandboxed),
      target_(target),
      app_path_(app_path),
      child_token_(mojo::edk::GenerateRandomToken()),
      start_child_process_event_(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      weak_factory_(this) {}

ChildProcessHost::~ChildProcessHost() {
  if (!app_path_.empty()) {
    CHECK(!mojo_ipc_channel_)
        << "Destroying ChildProcessHost before calling Join";
  }
}

mojom::ShellClientPtr ChildProcessHost::Start(
    const Identity& target,
    const ProcessReadyCallback& callback,
    const base::Closure& quit_closure) {
  DCHECK(!child_process_.IsValid());

  const base::CommandLine* parent_command_line =
      base::CommandLine::ForCurrentProcess();
  base::FilePath target_path = parent_command_line->GetProgram();
  // |app_path_| can be empty in tests.
  if (!app_path_.MatchesExtension(FILE_PATH_LITERAL(".mojo")) &&
      !app_path_.empty()) {
    target_path = app_path_;
  }

  std::unique_ptr<base::CommandLine> child_command_line(
      new base::CommandLine(target_path));

  child_command_line->AppendArguments(*parent_command_line, false);

#ifndef NDEBUG
  child_command_line->AppendSwitchASCII("n", target.name());
  child_command_line->AppendSwitchASCII("u", target.user_id());
#endif

  if (target_path != app_path_)
    child_command_line->AppendSwitchPath(switches::kChildProcess, app_path_);

  if (start_sandboxed_)
    child_command_line->AppendSwitch(switches::kEnableSandbox);

  mojo_ipc_channel_.reset(new mojo::edk::PlatformChannelPair);
  mojo_ipc_channel_->PrepareToPassClientHandleToChildProcess(
      child_command_line.get(), &handle_passing_info_);

  mojom::ShellClientPtr client =
      PassShellClientRequestOnCommandLine(child_command_line.get(),
                                          child_token_);
  launch_process_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ChildProcessHost::DoLaunch, base::Unretained(this),
                 base::Passed(&child_command_line)),
      base::Bind(&ChildProcessHost::DidStart,
                 weak_factory_.GetWeakPtr(), callback));
  return client;
}

void ChildProcessHost::Join() {
  if (mojo_ipc_channel_)
    start_child_process_event_.Wait();
  mojo_ipc_channel_.reset();
  if (child_process_.IsValid()) {
    int rv = -1;
    LOG_IF(ERROR, !child_process_.WaitForExit(&rv))
        << "Failed to wait for child process";
    child_process_.Close();
  }
}

void ChildProcessHost::DidStart(const ProcessReadyCallback& callback) {
  if (child_process_.IsValid()) {
    callback.Run(child_process_.Pid());
  } else {
    LOG(ERROR) << "Failed to start child process";
    mojo_ipc_channel_.reset();
    callback.Run(base::kNullProcessId);
  }
}

void ChildProcessHost::DoLaunch(
    std::unique_ptr<base::CommandLine> child_command_line) {
  if (delegate_) {
    delegate_->AdjustCommandLineArgumentsForTarget(target_,
                                                   child_command_line.get());
  }

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.handles_to_inherit = &handle_passing_info_;
#if defined(OFFICIAL_BUILD)
  CHECK(false) << "Launching mojo process with inherit_handles is insecure!";
#endif
  options.inherit_handles = true;
  options.stdin_handle = INVALID_HANDLE_VALUE;
  options.stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  options.stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
  // Always inherit stdout/stderr as a pair.
  if (!options.stdout_handle || !options.stdin_handle)
    options.stdin_handle = options.stdout_handle = nullptr;

  // Pseudo handles are used when stdout and stderr redirect to the console. In
  // that case, they're automatically inherited by child processes. See
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms682075.aspx
  // Trying to add them to the list of handles to inherit causes CreateProcess
  // to fail. When this process is launched from Python then a real handle is
  // used. In that case, we do want to add it to the list of handles that is
  // inherited.
  if (options.stdout_handle &&
      GetFileType(options.stdout_handle) != FILE_TYPE_CHAR) {
    handle_passing_info_.push_back(options.stdout_handle);
  }
  if (options.stderr_handle &&
      GetFileType(options.stderr_handle) != FILE_TYPE_CHAR &&
      options.stdout_handle != options.stderr_handle) {
    handle_passing_info_.push_back(options.stderr_handle);
  }
#elif defined(OS_POSIX)
  handle_passing_info_.push_back(std::make_pair(STDIN_FILENO, STDIN_FILENO));
  handle_passing_info_.push_back(std::make_pair(STDOUT_FILENO, STDOUT_FILENO));
  handle_passing_info_.push_back(std::make_pair(STDERR_FILENO, STDERR_FILENO));
  options.fds_to_remap = &handle_passing_info_;
#endif
  DVLOG(2) << "Launching child with command line: "
           << child_command_line->GetCommandLineString();
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  if (start_sandboxed_) {
    child_process_ =
        sandbox::NamespaceSandbox::LaunchProcess(*child_command_line, options);
    if (!child_process_.IsValid()) {
      LOG(ERROR) << "Starting the process with a sandbox failed. Missing kernel"
                 << " support.";
    }
  } else
#endif
  {
#if defined(OS_MACOSX)
    MachBroker* mach_broker = MachBroker::GetInstance();
    base::AutoLock locker(mach_broker->GetLock());
#endif
    child_process_ = base::LaunchProcess(*child_command_line, options);
#if defined(OS_MACOSX)
    mach_broker->ExpectPid(child_process_.Handle());
#endif
  }

  if (child_process_.IsValid()) {
    if (mojo_ipc_channel_.get()) {
      mojo_ipc_channel_->ChildProcessLaunched();
      mojo::edk::ChildProcessLaunched(
          child_process_.Handle(),
          mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(
              mojo_ipc_channel_->PassServerHandle().release().handle)),
          child_token_);
    }
  }
  start_child_process_event_.Signal();
}

}  // namespace shell
