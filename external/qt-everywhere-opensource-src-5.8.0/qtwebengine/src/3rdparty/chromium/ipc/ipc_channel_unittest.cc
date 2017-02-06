// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <stdint.h>

#include <memory>
#include <string>

#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "ipc/ipc_test_base.h"
#include "ipc/ipc_test_channel_listener.h"

namespace {

class IPCChannelTest : public IPCTestBase {
};

TEST_F(IPCChannelTest, ChannelTest) {
  Init("GenericClient");

  // Set up IPC channel and start client.
  IPC::TestChannelListener listener;
  CreateChannel(&listener);
  listener.Init(sender());
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  IPC::TestChannelListener::SendOneMessage(sender(), "hello from parent");

  // Run message loop.
  base::RunLoop().Run();

  // Close the channel so the client's OnChannelError() gets fired.
  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

// TODO(viettrungluu): Move to a separate IPCChannelWinTest.
#if defined(OS_WIN)
TEST_F(IPCChannelTest, ChannelTestExistingPipe) {
  Init("GenericClient");

  // Create pipe manually using the standard Chromium name and set up IPC
  // channel.
  IPC::TestChannelListener listener;
  std::string name("\\\\.\\pipe\\chrome.");
  name.append(GetChannelName("GenericClient"));
  HANDLE pipe = CreateNamedPipeA(name.c_str(),
                                 PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                                     FILE_FLAG_FIRST_PIPE_INSTANCE,
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                                 1,
                                 4096,
                                 4096,
                                 5000,
                                 NULL);
  CreateChannelFromChannelHandle(IPC::ChannelHandle(pipe), &listener);
  CloseHandle(pipe);  // The channel duplicates the handle.
  listener.Init(sender());

  // Connect to channel and start client.
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  IPC::TestChannelListener::SendOneMessage(sender(), "hello from parent");

  // Run message loop.
  base::MessageLoop::current()->Run();

  // Close the channel so the client's OnChannelError() gets fired.
  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}
#endif  // defined (OS_WIN)

TEST_F(IPCChannelTest, ChannelProxyTest) {
  Init("GenericClient");

  base::Thread thread("ChannelProxyTestServer");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread.StartWithOptions(options);

  // Set up IPC channel proxy.
  IPC::TestChannelListener listener;
  CreateChannelProxy(&listener, thread.task_runner().get());
  listener.Init(sender());

  ASSERT_TRUE(StartClient());

  IPC::TestChannelListener::SendOneMessage(sender(), "hello from parent");

  // Run message loop.
  base::RunLoop().Run();

  EXPECT_TRUE(WaitForClientShutdown());

  // Destroy the channel proxy before shutting down the thread.
  DestroyChannelProxy();
  thread.Stop();
}

class ChannelListenerWithOnConnectedSend : public IPC::TestChannelListener {
 public:
  ChannelListenerWithOnConnectedSend() {}
  ~ChannelListenerWithOnConnectedSend() override {}

  void OnChannelConnected(int32_t peer_pid) override {
    SendNextMessage();
  }
};

#if defined(OS_WIN)
// Acting flakey in Windows. http://crbug.com/129595
#define MAYBE_SendMessageInChannelConnected DISABLED_SendMessageInChannelConnected
#else
#define MAYBE_SendMessageInChannelConnected SendMessageInChannelConnected
#endif
// This tests the case of a listener sending back an event in its
// OnChannelConnected handler.
TEST_F(IPCChannelTest, MAYBE_SendMessageInChannelConnected) {
  Init("GenericClient");

  // Set up IPC channel and start client.
  ChannelListenerWithOnConnectedSend listener;
  CreateChannel(&listener);
  listener.Init(sender());
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  IPC::TestChannelListener::SendOneMessage(sender(), "hello from parent");

  // Run message loop.
  base::RunLoop().Run();

  // Close the channel so the client's OnChannelError() gets fired.
  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(GenericClient) {
  base::MessageLoopForIO main_message_loop;
  IPC::TestChannelListener listener;

  // Set up IPC channel.
  std::unique_ptr<IPC::Channel> channel(IPC::Channel::CreateClient(
      IPCTestBase::GetChannelName("GenericClient"), &listener));
  CHECK(channel->Connect());
  listener.Init(channel.get());
  IPC::TestChannelListener::SendOneMessage(channel.get(), "hello from child");

  base::RunLoop().Run();
  return 0;
}

}  // namespace
