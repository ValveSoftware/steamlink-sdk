// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This file also tests child_process.*.

#include "services/shell/runner/host/child_process_host.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "services/shell/native_runner_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shell {
namespace {

void ProcessReadyCallbackAdapater(const base::Closure& callback,
                                  base::ProcessId process_id) {
  callback.Run();
}

class ProcessDelegate : public mojo::edk::ProcessDelegate {
 public:
  ProcessDelegate() {}
  ~ProcessDelegate() override {}

 private:
  void OnShutdownComplete() override {}
  DISALLOW_COPY_AND_ASSIGN(ProcessDelegate);
};

class NativeRunnerDelegateImpl : public NativeRunnerDelegate {
 public:
  NativeRunnerDelegateImpl() {}
  ~NativeRunnerDelegateImpl() override {}

  size_t get_and_clear_adjust_count() {
    size_t count = 0;
    std::swap(count, adjust_count_);
    return count;
  }

 private:
  // NativeRunnerDelegate:
  void AdjustCommandLineArgumentsForTarget(
      const Identity& target,
      base::CommandLine* command_line) override {
    adjust_count_++;
  }

  size_t adjust_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NativeRunnerDelegateImpl);
};

#if defined(OS_ANDROID)
// TODO(qsr): Multiprocess shell tests are not supported on android.
#define MAYBE_StartJoin DISABLED_StartJoin
#else
#define MAYBE_StartJoin StartJoin
#endif  // defined(OS_ANDROID)
// Just tests starting the child process and joining it (without starting an
// app).
TEST(ChildProcessHostTest, MAYBE_StartJoin) {
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);
  base::MessageLoop message_loop;
  scoped_refptr<base::SequencedWorkerPool> blocking_pool(
      new base::SequencedWorkerPool(3, "blocking_pool"));

  base::Thread io_thread("io_thread");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  ProcessDelegate delegate;
  mojo::edk::InitIPCSupport(&delegate, io_thread.task_runner());

  NativeRunnerDelegateImpl native_runner_delegate;
  ChildProcessHost child_process_host(blocking_pool.get(),
                                      &native_runner_delegate, false,
                                      Identity(), base::FilePath());
  base::RunLoop run_loop;
  child_process_host.Start(
      Identity(),
      base::Bind(&ProcessReadyCallbackAdapater, run_loop.QuitClosure()),
      base::Bind(&base::DoNothing));
  run_loop.Run();

  child_process_host.Join();
  blocking_pool->Shutdown();
  mojo::edk::ShutdownIPCSupport();
  EXPECT_EQ(1u, native_runner_delegate.get_and_clear_adjust_count());
}

}  // namespace
}  // namespace shell
