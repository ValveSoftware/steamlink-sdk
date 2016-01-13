// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/threading/thread.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_base.h"

namespace {

const size_t kLongMessageStringNumBytes = 50000;

static void Send(IPC::Sender* sender, const char* text) {
  static int message_index = 0;

  IPC::Message* message = new IPC::Message(0,
                                           2,
                                           IPC::Message::PRIORITY_NORMAL);
  message->WriteInt(message_index++);
  message->WriteString(std::string(text));

  // Make sure we can handle large messages.
  char junk[kLongMessageStringNumBytes];
  memset(junk, 'a', sizeof(junk)-1);
  junk[sizeof(junk)-1] = 0;
  message->WriteString(std::string(junk));

  // DEBUG: printf("[%u] sending message [%s]\n", GetCurrentProcessId(), text);
  sender->Send(message);
}

// A generic listener that expects messages of a certain type (see
// OnMessageReceived()), and either sends a generic response or quits after the
// 50th message (or on channel error).
class GenericChannelListener : public IPC::Listener {
 public:
  GenericChannelListener() : sender_(NULL), messages_left_(50) {}
  virtual ~GenericChannelListener() {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    PickleIterator iter(message);

    int ignored;
    EXPECT_TRUE(iter.ReadInt(&ignored));
    std::string data;
    EXPECT_TRUE(iter.ReadString(&data));
    std::string big_string;
    EXPECT_TRUE(iter.ReadString(&big_string));
    EXPECT_EQ(kLongMessageStringNumBytes - 1, big_string.length());

    SendNextMessage();
    return true;
  }

  virtual void OnChannelError() OVERRIDE {
    // There is a race when closing the channel so the last message may be lost.
    EXPECT_LE(messages_left_, 1);
    base::MessageLoop::current()->Quit();
  }

  void Init(IPC::Sender* s) {
    sender_ = s;
  }

 protected:
  void SendNextMessage() {
    if (--messages_left_ <= 0)
      base::MessageLoop::current()->Quit();
    else
      Send(sender_, "Foo");
  }

 private:
  IPC::Sender* sender_;
  int messages_left_;
};

class IPCChannelTest : public IPCTestBase {
};

// TODO(viettrungluu): Move to a separate IPCMessageTest.
TEST_F(IPCChannelTest, BasicMessageTest) {
  int v1 = 10;
  std::string v2("foobar");
  std::wstring v3(L"hello world");

  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(v1));
  EXPECT_TRUE(m.WriteString(v2));
  EXPECT_TRUE(m.WriteWString(v3));

  PickleIterator iter(m);

  int vi;
  std::string vs;
  std::wstring vw;

  EXPECT_TRUE(m.ReadInt(&iter, &vi));
  EXPECT_EQ(v1, vi);

  EXPECT_TRUE(m.ReadString(&iter, &vs));
  EXPECT_EQ(v2, vs);

  EXPECT_TRUE(m.ReadWString(&iter, &vw));
  EXPECT_EQ(v3, vw);

  // should fail
  EXPECT_FALSE(m.ReadInt(&iter, &vi));
  EXPECT_FALSE(m.ReadString(&iter, &vs));
  EXPECT_FALSE(m.ReadWString(&iter, &vw));
}

TEST_F(IPCChannelTest, ChannelTest) {
  Init("GenericClient");

  // Set up IPC channel and start client.
  GenericChannelListener listener;
  CreateChannel(&listener);
  listener.Init(sender());
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  Send(sender(), "hello from parent");

  // Run message loop.
  base::MessageLoop::current()->Run();

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
  GenericChannelListener listener;
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

  Send(sender(), "hello from parent");

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
  GenericChannelListener listener;
  CreateChannelProxy(&listener, thread.message_loop_proxy().get());
  listener.Init(sender());

  ASSERT_TRUE(StartClient());

  Send(sender(), "hello from parent");

  // Run message loop.
  base::MessageLoop::current()->Run();

  EXPECT_TRUE(WaitForClientShutdown());

  // Destroy the channel proxy before shutting down the thread.
  DestroyChannelProxy();
  thread.Stop();
}

class ChannelListenerWithOnConnectedSend : public GenericChannelListener {
 public:
  ChannelListenerWithOnConnectedSend() {}
  virtual ~ChannelListenerWithOnConnectedSend() {}

  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE {
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

  Send(sender(), "hello from parent");

  // Run message loop.
  base::MessageLoop::current()->Run();

  // Close the channel so the client's OnChannelError() gets fired.
  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(GenericClient) {
  base::MessageLoopForIO main_message_loop;
  GenericChannelListener listener;

  // Set up IPC channel.
  scoped_ptr<IPC::Channel> channel(IPC::Channel::CreateClient(
      IPCTestBase::GetChannelName("GenericClient"),
      &listener));
  CHECK(channel->Connect());
  listener.Init(channel.get());
  Send(channel.get(), "hello from child");

  base::MessageLoop::current()->Run();
  return 0;
}

}  // namespace
