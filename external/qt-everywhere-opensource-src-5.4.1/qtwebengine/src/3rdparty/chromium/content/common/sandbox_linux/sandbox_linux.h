// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_LINUX_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/common/sandbox_linux.h"

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(LEAK_SANITIZER)
#include <sanitizer/common_interface_defs.h>
#endif

template <typename T> struct DefaultSingletonTraits;
namespace base {
class Thread;
}
namespace sandbox { class SetuidSandboxClient; }

namespace content {

// A singleton class to represent and change our sandboxing state for the
// three main Linux sandboxes.
class LinuxSandbox {
 public:
  // This is a list of sandbox IPC methods which the renderer may send to the
  // sandbox host. See http://code.google.com/p/chromium/wiki/LinuxSandboxIPC
  // This isn't the full list, values < 32 are reserved for methods called from
  // Skia.
  enum LinuxSandboxIPCMethods {
    METHOD_GET_FALLBACK_FONT_FOR_CHAR = 32,
    METHOD_LOCALTIME = 33,
    DEPRECATED_METHOD_GET_CHILD_WITH_INODE = 34,
    METHOD_GET_STYLE_FOR_STRIKE = 35,
    METHOD_MAKE_SHARED_MEMORY_SEGMENT = 36,
    METHOD_MATCH_WITH_FALLBACK = 37,
  };

  // Get our singleton instance.
  static LinuxSandbox* GetInstance();

  // Do some initialization that can only be done before any of the sandboxes
  // are enabled. If using the setuid sandbox, this should be called manually
  // before the setuid sandbox is engaged.
  void PreinitializeSandbox();

  // Initialize the sandbox with the given pre-built configuration. Currently
  // seccomp-bpf and address space limitations (the setuid sandbox works
  // differently and is set-up in the Zygote). This will instantiate the
  // LinuxSandbox singleton if it doesn't already exist.
  // This function should only be called without any thread running.
  static bool InitializeSandbox();

  // Stop |thread| in a way that can be trusted by the sandbox.
  static void StopThread(base::Thread* thread);

  // Returns the status of the renderer, worker and ppapi sandbox. Can only
  // be queried after going through PreinitializeSandbox(). This is a bitmask
  // and uses the constants defined in "enum LinuxSandboxStatus". Since the
  // status needs to be provided before the sandboxes are actually started,
  // this returns what will actually happen once InitializeSandbox()
  // is called from inside these processes.
  int GetStatus();
  // Returns true if the current process is single-threaded or if the number
  // of threads cannot be determined.
  bool IsSingleThreaded() const;
  // Did we start Seccomp BPF?
  bool seccomp_bpf_started() const;

  // Simple accessor for our instance of the setuid sandbox. Will never return
  // NULL.
  // There is no StartSetuidSandbox(), the SetuidSandboxClient instance should
  // be used directly.
  sandbox::SetuidSandboxClient* setuid_sandbox_client() const;

  // Check the policy and eventually start the seccomp-bpf sandbox. This should
  // never be called with threads started. If we detect that threads have
  // started we will crash.
  bool StartSeccompBPF(const std::string& process_type);

  // Limit the address space of the current process (and its children).
  // to make some vulnerabilities harder to exploit.
  bool LimitAddressSpace(const std::string& process_type);

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(LEAK_SANITIZER)
  __sanitizer_sandbox_arguments* sanitizer_args() const {
    return sanitizer_args_.get();
  };
#endif

 private:
  friend struct DefaultSingletonTraits<LinuxSandbox>;

  LinuxSandbox();
  ~LinuxSandbox();

  // Some methods are static and get an instance of the Singleton. These
  // are the non-static implementations.
  bool InitializeSandboxImpl();
  void StopThreadImpl(base::Thread* thread);
  // We must have been pre_initialized_ before using this.
  bool seccomp_bpf_supported() const;
  // Returns true if it can be determined that the current process has open
  // directories that are not managed by the LinuxSandbox class. This would
  // be a vulnerability as it would allow to bypass the setuid sandbox.
  bool HasOpenDirectories() const;
  // The last part of the initialization is to make sure any temporary "hole"
  // in the sandbox is closed. For now, this consists of closing proc_fd_.
  void SealSandbox();
  // GetStatus() makes promises as to how the sandbox will behave. This
  // checks that no promises have been broken.
  void CheckForBrokenPromises(const std::string& process_type);
  // Stop |thread| and make sure it does not appear in /proc/self/tasks/
  // anymore.
  void StopThreadAndEnsureNotCounted(base::Thread* thread) const;

  // A file descriptor to /proc. It's dangerous to have it around as it could
  // allow for sandbox bypasses. It needs to be closed before we consider
  // ourselves sandboxed.
  int proc_fd_;
  bool seccomp_bpf_started_;
  // The value returned by GetStatus(). Gets computed once and then cached.
  int sandbox_status_flags_;
  // Did PreinitializeSandbox() run?
  bool pre_initialized_;
  bool seccomp_bpf_supported_;  // Accurate if pre_initialized_.
  bool yama_is_enforcing_;  // Accurate if pre_initialized_.
  scoped_ptr<sandbox::SetuidSandboxClient> setuid_sandbox_client_;
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(LEAK_SANITIZER)
  scoped_ptr<__sanitizer_sandbox_arguments> sanitizer_args_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LinuxSandbox);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_LINUX_H_
