// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_bootstrap.h"

#include <stdint.h>
#include <memory>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_test_base.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "mojo/edk/test/scoped_ipc_support.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace {

class IPCMojoBootstrapTest : public testing::Test {
 protected:
  mojo::edk::test::MultiprocessTestHelper helper_;
};

class TestingDelegate : public IPC::MojoBootstrap::Delegate {
 public:
  explicit TestingDelegate(const base::Closure& quit_callback)
      : passed_(false), quit_callback_(quit_callback) {}

  void OnPipesAvailable(IPC::mojom::ChannelAssociatedPtrInfo send_channel,
                        IPC::mojom::ChannelAssociatedRequest receive_channel,
                        int32_t peer_pid) override;
  void OnBootstrapError() override;

  bool passed() const { return passed_; }

 private:
  bool passed_;
  const base::Closure quit_callback_;
};

void TestingDelegate::OnPipesAvailable(
    IPC::mojom::ChannelAssociatedPtrInfo send_channel,
    IPC::mojom::ChannelAssociatedRequest receive_channel,
    int32_t peer_pid) {
  passed_ = true;
  quit_callback_.Run();
}

void TestingDelegate::OnBootstrapError() {
  quit_callback_.Run();
}

TEST_F(IPCMojoBootstrapTest, Connect) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  TestingDelegate delegate(run_loop.QuitClosure());
  std::unique_ptr<IPC::MojoBootstrap> bootstrap = IPC::MojoBootstrap::Create(
      helper_.StartChild("IPCMojoBootstrapTestClient"),
      IPC::Channel::MODE_SERVER, &delegate);

  bootstrap->Connect();
  run_loop.Run();

  EXPECT_TRUE(delegate.passed());
  EXPECT_TRUE(helper_.WaitForChildTestShutdown());
}

// A long running process that connects to us.
MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    IPCMojoBootstrapTestClientTestChildMain,
    ::mojo::edk::test::MultiprocessTestHelper::ChildSetup) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  TestingDelegate delegate(run_loop.QuitClosure());
  std::unique_ptr<IPC::MojoBootstrap> bootstrap = IPC::MojoBootstrap::Create(
      mojo::edk::CreateChildMessagePipe(
          mojo::edk::test::MultiprocessTestHelper::primordial_pipe_token),
      IPC::Channel::MODE_CLIENT, &delegate);

  bootstrap->Connect();

  run_loop.Run();

  return delegate.passed() ? 0 : 1;
}

}  // namespace
