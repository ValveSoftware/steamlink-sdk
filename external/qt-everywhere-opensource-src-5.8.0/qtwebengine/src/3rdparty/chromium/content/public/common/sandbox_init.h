// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOX_INIT_H_
#define CONTENT_PUBLIC_COMMON_SANDBOX_INIT_H_

#include <memory>

#include "base/files/scoped_file.h"
#include "base/memory/shared_memory.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace base {
class CommandLine;
class FilePath;
}

namespace sandbox {
namespace bpf_dsl {
class Policy;
}
struct SandboxInterfaceInfo;
enum ResultCode : int;
}

namespace content {
class SandboxedProcessLauncherDelegate;

#if defined(OS_WIN)

// Initialize the sandbox for renderer, gpu, utility, worker, nacl, and plugin
// processes, depending on the command line flags. Although The browser process
// is not sandboxed, this also needs to be called because it will initialize
// the broker code.
// Returns true if the sandbox was initialized succesfully, false if an error
// occurred.  If process_type isn't one that needs sandboxing true is always
// returned.
CONTENT_EXPORT bool InitializeSandbox(
    sandbox::SandboxInterfaceInfo* sandbox_info);

// Launch a sandboxed process. |delegate| may be NULL. If |delegate| is non-NULL
// then it just has to outlive this method call. |handles_to_inherit| is a list
// of handles for the child process to inherit. The caller retains ownership of
// the handles.
CONTENT_EXPORT sandbox::ResultCode StartSandboxedProcess(
    SandboxedProcessLauncherDelegate* delegate,
    base::CommandLine* cmd_line,
    const base::HandlesToInheritVector& handles_to_inherit,
    base::Process* process);

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

#elif defined(OS_LINUX) || defined(OS_NACL_NONSFI)

class SandboxInitializerDelegate;

// Initialize a seccomp-bpf sandbox. |policy| may not be NULL.
// If an existing layer of sandboxing is present that would prevent access to
// /proc, |proc_fd| must be a valid file descriptor to /proc/.
// Returns true if the sandbox has been properly engaged.
CONTENT_EXPORT bool InitializeSandbox(
    std::unique_ptr<sandbox::bpf_dsl::Policy> policy,
    base::ScopedFD proc_fd);

// Return a "baseline" policy. This is used by a SandboxInitializerDelegate to
// implement a policy that is derived from the baseline.
CONTENT_EXPORT std::unique_ptr<sandbox::bpf_dsl::Policy>
GetBPFSandboxBaselinePolicy();
#endif  // defined(OS_LINUX) || defined(OS_NACL_NONSFI)

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOX_INIT_H_
