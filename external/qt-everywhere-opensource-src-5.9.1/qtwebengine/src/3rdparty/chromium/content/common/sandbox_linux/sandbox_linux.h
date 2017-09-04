// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_LINUX_H_

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "content/public/common/sandbox_linux.h"

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(UNDEFINED_SANITIZER) || defined(SANITIZER_COVERAGE)
#include <sanitizer/common_interface_defs.h>
#define ANY_OF_AMTLU_SANITIZER 1
#endif

namespace base {
template <typename T>
struct DefaultSingletonTraits;
class Thread;
}
namespace sandbox { class SetuidSandboxClient; }

namespace content {

// A singleton class to represent and change our sandboxing state for the
// three main Linux sandboxes.
// The sandboxing model allows using two layers of sandboxing. The first layer
// can be implemented either with unprivileged namespaces or with the setuid
// sandbox. This class provides a way to engage the namespace sandbox, but does
// not deal with the legacy setuid sandbox directly.
// The second layer is mainly based on seccomp-bpf and is engaged with
// InitializeSandbox(). InitializeSandbox() is also responsible for "sealing"
// the first layer of sandboxing. That is, InitializeSandbox must always be
// called to have any meaningful sandboxing at all.
class LinuxSandbox {
 public:
  // This is a list of sandbox IPC methods which the renderer may send to the
  // sandbox host. See https://chromium.googlesource.com/chromium/src/+/master/docs/linux_sandbox_ipc.md
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
  // Security: When this runs, it is imperative that either InitializeSandbox()
  // runs as well or that all file descriptors returned in
  // GetFileDescriptorsToClose() get closed.
  // Otherwise file descriptors that bypass the security of the setuid sandbox
  // would be kept open. One must be particularly careful if a process performs
  // a fork().
  void PreinitializeSandbox();

  // Check that the current process is the init process of a new PID
  // namespace and then proceed to drop access to the file system by using
  // a new unprivileged namespace. This is a layer-1 sandbox.
  // In order for this sandbox to be effective, it must be "sealed" by calling
  // InitializeSandbox().
  void EngageNamespaceSandbox();

  // Return a list of file descriptors to close if PreinitializeSandbox() ran
  // but InitializeSandbox() won't. Avoid using.
  // TODO(jln): get rid of this hack.
  std::vector<int> GetFileDescriptorsToClose();

  // Seal an eventual layer-1 sandbox and initialize the layer-2 sandbox with
  // an adequate policy depending on the process type and command line
  // arguments.
  // Currently the layer-2 sandbox is composed of seccomp-bpf and address space
  // limitations. This will instantiate the LinuxSandbox singleton if it
  // doesn't already exist.
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

  // Returns a file descriptor to proc. The file descriptor is no longer valid
  // after the sandbox has been sealed.
  int proc_fd() const {
    DCHECK_NE(-1, proc_fd_);
    return proc_fd_;
  }

#if defined(ANY_OF_AMTLU_SANITIZER)
  __sanitizer_sandbox_arguments* sanitizer_args() const {
    return sanitizer_args_.get();
  };
#endif

 private:
  friend struct base::DefaultSingletonTraits<LinuxSandbox>;

  LinuxSandbox();
  ~LinuxSandbox();

  // Some methods are static and get an instance of the Singleton. These
  // are the non-static implementations.
  bool InitializeSandboxImpl();
  void StopThreadImpl(base::Thread* thread);
  // We must have been pre_initialized_ before using these.
  bool seccomp_bpf_supported() const;
  bool seccomp_bpf_with_tsync_supported() const;
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
  bool seccomp_bpf_with_tsync_supported_;  // Accurate if pre_initialized_.
  bool yama_is_enforcing_;  // Accurate if pre_initialized_.
  bool initialize_sandbox_ran_;  // InitializeSandbox() was called.
  std::unique_ptr<sandbox::SetuidSandboxClient> setuid_sandbox_client_;
#if defined(ANY_OF_AMTLU_SANITIZER)
  std::unique_ptr<__sanitizer_sandbox_arguments> sanitizer_args_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LinuxSandbox);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_LINUX_H_
