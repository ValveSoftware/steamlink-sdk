// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
#define CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_

#include "base/environment.h"
#include "base/process/process.h"

#include "content/common/content_export.h"

#if defined(OS_MACOSX)
#include "content/public/common/sandbox_type_mac.h"
#endif

namespace base {
class FilePath;
}

namespace sandbox {
class TargetPolicy;
}

namespace content {

// Allows a caller of StartSandboxedProcess or
// BrowserChildProcessHost/ChildProcessLauncher to control the sandbox policy,
// i.e. to loosen it if needed.
// The methods below will be called on the PROCESS_LAUNCHER thread.
class CONTENT_EXPORT SandboxedProcessLauncherDelegate {
 public:
  virtual ~SandboxedProcessLauncherDelegate() {}

#if defined(OS_WIN)
  // Override to return true if the process should be launched as an elevated
  // process (which implies no sandbox).
  virtual bool ShouldLaunchElevated();

  // By default, the process is launched sandboxed. Override this method to
  // return false if the process should be launched without a sandbox
  // (i.e. through base::LaunchProcess directly).
  virtual bool ShouldSandbox();

  // Called before the default sandbox is applied. If the default policy is too
  // restrictive, the caller should set |disable_default_policy| to true and
  // apply their policy in PreSpawnTarget. |exposed_dir| is used to allow a
  //directory through the sandbox.
  virtual void PreSandbox(bool* disable_default_policy,
                          base::FilePath* exposed_dir) {}

  // Called right before spawning the process.
  virtual void PreSpawnTarget(sandbox::TargetPolicy* policy,
                              bool* success) {}

  // Called right after the process is launched, but before its thread is run.
  virtual void PostSpawnTarget(base::ProcessHandle process) {}

#elif defined(OS_POSIX)
  // Override this to return true to use the setuid sandbox.
  virtual bool ShouldUseZygote();

  // Override this if the process needs a non-empty environment map.
  virtual base::EnvironmentMap GetEnvironment();

  // Return the file descriptor for the IPC channel.
  virtual int GetIpcFd() = 0;

#if defined(OS_MACOSX)
  // Gets the Mac SandboxType to enforce on the process. Return
  // SANDBOX_TYPE_INVALID for no sandbox policy.
  virtual SandboxType GetSandboxType();
#endif

#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
