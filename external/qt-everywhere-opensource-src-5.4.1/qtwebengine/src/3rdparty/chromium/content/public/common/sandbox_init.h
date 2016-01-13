// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOX_INIT_H_
#define CONTENT_PUBLIC_COMMON_SANDBOX_INIT_H_

#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace base {
class CommandLine;
class FilePath;
}

namespace sandbox {
class SandboxBPFPolicy;
struct SandboxInterfaceInfo;
}

namespace content {
class SandboxedProcessLauncherDelegate;

#if defined(OS_WIN)

// Initialize the sandbox for renderer, gpu, utility, worker, nacl, and plug-in
// processes, depending on the command line flags. Although The browser process
// is not sandboxed, this also needs to be called because it will initialize
// the broker code.
// Returns true if the sandbox was initialized succesfully, false if an error
// occurred.  If process_type isn't one that needs sandboxing true is always
// returned.
CONTENT_EXPORT bool InitializeSandbox(
    sandbox::SandboxInterfaceInfo* sandbox_info);

// This is a restricted version of Windows' DuplicateHandle() function
// that works inside the sandbox and can send handles but not retrieve
// them.  Unlike DuplicateHandle(), it takes a process ID rather than
// a process handle.  It returns true on success, false otherwise.
CONTENT_EXPORT bool BrokerDuplicateHandle(HANDLE source_handle,
                                          DWORD target_process_id,
                                          HANDLE* target_handle,
                                          DWORD desired_access,
                                          DWORD options);

// Inform the current process's sandbox broker (e.g. the broker for
// 32-bit processes) about a process created under a different sandbox
// broker (e.g. the broker for 64-bit processes).  This allows
// BrokerDuplicateHandle() to send handles to a process managed by
// another broker.  For example, it allows the 32-bit renderer to send
// handles to 64-bit NaCl processes.  This returns true on success,
// false otherwise.
CONTENT_EXPORT bool BrokerAddTargetPeer(HANDLE peer_process);

// Launch a sandboxed process. |delegate| may be NULL. If |delegate| is non-NULL
// then it just has to outlive this method call.
CONTENT_EXPORT base::ProcessHandle StartSandboxedProcess(
    SandboxedProcessLauncherDelegate* delegate,
    base::CommandLine* cmd_line);

#elif defined(OS_MACOSX)

// Initialize the sandbox of the given |sandbox_type|, optionally specifying a
// directory to allow access to. Note specifying a directory needs to be
// supported by the sandbox profile associated with the given |sandbox_type|.
// Valid values for |sandbox_type| are defined either by the enum SandboxType,
// or by ContentClient::GetSandboxProfileForSandboxType().
//
// If the |sandbox_type| isn't one of the ones defined by content then the
// embedder is queried using ContentClient::GetSandboxPolicyForSandboxType().
// The embedder can use values for |sandbox_type| starting from
// sandbox::SANDBOX_PROCESS_TYPE_AFTER_LAST_TYPE.
//
// Returns true if the sandbox was initialized succesfully, false if an error
// occurred.  If process_type isn't one that needs sandboxing, no action is
// taken and true is always returned.
CONTENT_EXPORT bool InitializeSandbox(int sandbox_type,
                                      const base::FilePath& allowed_path);

#elif defined(OS_LINUX)

class SandboxInitializerDelegate;

// Initialize a seccomp-bpf sandbox. |policy| may not be NULL.
// Returns true if the sandbox has been properly engaged.
CONTENT_EXPORT bool InitializeSandbox(
    scoped_ptr<sandbox::SandboxBPFPolicy> policy);

// Return a "baseline" policy. This is used by a SandboxInitializerDelegate to
// implement a policy that is derived from the baseline.
CONTENT_EXPORT scoped_ptr<sandbox::SandboxBPFPolicy>
GetBPFSandboxBaselinePolicy();
#endif  // defined(OS_LINUX)

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOX_INIT_H_
