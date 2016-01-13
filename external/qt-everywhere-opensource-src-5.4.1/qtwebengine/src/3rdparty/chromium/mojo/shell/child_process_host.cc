// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/child_process_host.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "mojo/shell/context.h"
#include "mojo/shell/switches.h"

namespace mojo {
namespace shell {

ChildProcessHost::ChildProcessHost(Context* context,
                                   Delegate* delegate,
                                   ChildProcess::Type type)
    : context_(context),
      delegate_(delegate),
      type_(type),
      child_process_handle_(base::kNullProcessHandle) {
  DCHECK(delegate);
  platform_channel_ = platform_channel_pair_.PassServerHandle();
  CHECK(platform_channel_.is_valid());
}

ChildProcessHost::~ChildProcessHost() {
  if (child_process_handle_ != base::kNullProcessHandle) {
    LOG(WARNING) << "Destroying ChildProcessHost with unjoined child";
    base::CloseProcessHandle(child_process_handle_);
    child_process_handle_ = base::kNullProcessHandle;
  }
}

void ChildProcessHost::Start() {
  DCHECK_EQ(child_process_handle_, base::kNullProcessHandle);

  delegate_->WillStart();

  CHECK(base::PostTaskAndReplyWithResult(
      context_->task_runners()->blocking_pool(),
      FROM_HERE,
      base::Bind(&ChildProcessHost::DoLaunch, base::Unretained(this)),
      base::Bind(&ChildProcessHost::DidLaunch, base::Unretained(this))));
}

int ChildProcessHost::Join() {
  DCHECK_NE(child_process_handle_, base::kNullProcessHandle);
  int rv = -1;
  LOG_IF(ERROR, !base::WaitForExitCode(child_process_handle_, &rv))
      << "Failed to wait for child process";
  base::CloseProcessHandle(child_process_handle_);
  child_process_handle_ = base::kNullProcessHandle;
  return rv;
}

bool ChildProcessHost::DoLaunch() {
  static const char* kForwardSwitches[] = {
    switches::kTraceToConsole,
    switches::kV,
    switches::kVModule,
  };

  const base::CommandLine* parent_command_line =
      base::CommandLine::ForCurrentProcess();
  base::CommandLine child_command_line(parent_command_line->GetProgram());
  child_command_line.CopySwitchesFrom(*parent_command_line, kForwardSwitches,
                                      arraysize(kForwardSwitches));
  child_command_line.AppendSwitchASCII(
      switches::kChildProcessType, base::IntToString(static_cast<int>(type_)));

  embedder::HandlePassingInformation handle_passing_info;
  platform_channel_pair_.PrepareToPassClientHandleToChildProcess(
      &child_command_line, &handle_passing_info);

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
  options.handles_to_inherit = &handle_passing_info;
#elif defined(OS_POSIX)
  options.fds_to_remap = &handle_passing_info;
#endif

  if (!base::LaunchProcess(child_command_line, options, &child_process_handle_))
    return false;

  platform_channel_pair_.ChildProcessLaunched();
  return true;
}

void ChildProcessHost::DidLaunch(bool success) {
  delegate_->DidStart(success);
}

}  // namespace shell
}  // namespace mojo
