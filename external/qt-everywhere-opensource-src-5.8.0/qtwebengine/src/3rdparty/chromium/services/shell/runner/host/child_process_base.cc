// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/runner/host/child_process_base.h"

#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "services/shell/runner/common/client_util.h"
#include "services/shell/runner/common/switches.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "services/shell/runner/host/linux_sandbox.h"
#endif

#if defined(OS_MACOSX)
#include "services/shell/runner/host/mach_broker.h"
#endif

namespace shell {

namespace {

#if defined(OS_LINUX) && !defined(OS_ANDROID)
std::unique_ptr<LinuxSandbox> InitializeSandbox() {
  using sandbox::syscall_broker::BrokerFilePermission;
  // Warm parts of base in the copy of base in the mojo runner.
  base::RandUint64();
  base::SysInfo::AmountOfPhysicalMemory();
  base::SysInfo::NumberOfProcessors();

  // TODO(erg,jln): Allowing access to all of /dev/shm/ makes it easy to
  // spy on other shared memory using processes. This is a temporary hack
  // so that we have some sandbox until we have proper shared memory
  // support integrated into mojo.
  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(
      BrokerFilePermission::ReadWriteCreateUnlinkRecursive("/dev/shm/"));
  std::unique_ptr<LinuxSandbox> sandbox(new LinuxSandbox(permissions));
  sandbox->Warmup();
  sandbox->EngageNamespaceSandbox();
  sandbox->EngageSeccompSandbox();
  sandbox->Seal();
  return sandbox;
}
#endif

// Should be created and initialized on the main thread and kept alive as long
// a Mojo application is running in the current process.
class ScopedAppContext : public mojo::edk::ProcessDelegate {
 public:
  ScopedAppContext()
      : io_thread_("io_thread"),
        wait_for_shutdown_event_(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {
    // Initialize Mojo before starting any threads.
    mojo::edk::Init();

    // Create and start our I/O thread.
    base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread_.StartWithOptions(io_thread_options));
    io_runner_ = io_thread_.task_runner().get();
    CHECK(io_runner_.get());

    mojo::edk::InitIPCSupport(this, io_runner_);
    mojo::edk::SetParentPipeHandleFromCommandLine();
  }

  ~ScopedAppContext() override {
    mojo::edk::ShutdownIPCSupport();
    wait_for_shutdown_event_.Wait();
  }

 private:
  // ProcessDelegate implementation.
  void OnShutdownComplete() override {
    wait_for_shutdown_event_.Signal();
  }

  base::Thread io_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  // Used to unblock the main thread on shutdown.
  base::WaitableEvent wait_for_shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAppContext);
};

}  // namespace

void ChildProcessMainWithCallback(const RunCallback& callback) {
  DCHECK(!base::MessageLoop::current());

#if defined(OS_MACOSX)
  // Send our task port to the parent.
  MachBroker::SendTaskPortToParent();
#endif

#if !defined(OFFICIAL_BUILD)
  // Initialize stack dumping just before initializing sandbox to make
  // sure symbol names in all loaded libraries will be cached.
  base::debug::EnableInProcessStackDumping();
#endif
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  std::unique_ptr<LinuxSandbox> sandbox;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableSandbox))
    sandbox = InitializeSandbox();
#endif

  ScopedAppContext app_context;
  callback.Run(GetShellClientRequestFromCommandLine());
}

}  // namespace shell
