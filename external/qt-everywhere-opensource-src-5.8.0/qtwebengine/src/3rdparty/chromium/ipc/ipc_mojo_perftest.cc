// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_perftest_support.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "mojo/edk/test/scoped_ipc_support.h"

namespace IPC {
namespace {

class MojoChannelPerfTest : public test::IPCChannelPerfTestBase {
 public:
  void TearDown() override {
    ipc_support_.reset();
    test::IPCChannelPerfTestBase::TearDown();
  }

  std::unique_ptr<ChannelFactory> CreateChannelFactory(
      const ChannelHandle& handle,
      base::SequencedTaskRunner* runner) override {
    ipc_support_.reset(new mojo::edk::test::ScopedIPCSupport(io_task_runner()));
    return ChannelMojo::CreateServerFactory(
        helper_.StartChild("MojoPerfTestClient"));
  }

  bool StartClient() override {
    return true;
  }

  bool WaitForClientShutdown() override {
    return helper_.WaitForChildTestShutdown();
  }

  mojo::edk::test::MultiprocessTestHelper helper_;
  std::unique_ptr<mojo::edk::test::ScopedIPCSupport> ipc_support_;
};

TEST_F(MojoChannelPerfTest, ChannelPingPong) {
  RunTestChannelPingPong(GetDefaultTestParams());

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(MojoChannelPerfTest, ChannelProxyPingPong) {
  RunTestChannelProxyPingPong(GetDefaultTestParams());

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// Test to see how many channels we can create.
TEST_F(MojoChannelPerfTest, DISABLED_MaxChannelCount) {
#if defined(OS_POSIX)
  LOG(INFO) << "base::GetMaxFds " << base::GetMaxFds();
  base::SetFdLimit(20000);
#endif

  std::vector<mojo::edk::PlatformChannelPair*> channels;
  for (size_t i = 0; i < 10000; ++i) {
    LOG(INFO) << "channels size: " << channels.size();
    channels.push_back(new mojo::edk::PlatformChannelPair());
  }
}

class MojoPerfTestClient : public test::PingPongTestClient {
 public:
  typedef test::PingPongTestClient SuperType;

  MojoPerfTestClient();

  std::unique_ptr<Channel> CreateChannel(Listener* listener) override;

  int Run(MojoHandle handle);

 private:
  mojo::edk::test::ScopedIPCSupport ipc_support_;
  mojo::ScopedMessagePipeHandle handle_;
};

MojoPerfTestClient::MojoPerfTestClient()
    : ipc_support_(base::ThreadTaskRunnerHandle::Get()) {
  mojo::edk::test::MultiprocessTestHelper::ChildSetup();
}

std::unique_ptr<Channel> MojoPerfTestClient::CreateChannel(Listener* listener) {
  return ChannelMojo::Create(std::move(handle_), Channel::MODE_CLIENT,
                             listener);
}

int MojoPerfTestClient::Run(MojoHandle handle) {
  handle_ = mojo::MakeScopedHandle(mojo::MessagePipeHandle(handle));
  return RunMain();
}

MULTIPROCESS_TEST_MAIN(MojoPerfTestClientTestChildMain) {
  MojoPerfTestClient client;
  int rv = mojo::edk::test::MultiprocessTestHelper::RunClientMain(
      base::Bind(&MojoPerfTestClient::Run, base::Unretained(&client)));

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  return rv;
}

}  // namespace
}  // namespace IPC
