// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_browser_associated_interfaces.mojom.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const int kNumTestMessages = 100;

class BrowserAssociatedInterfaceTest : public testing::Test {
 public:
  static void AddFilterToChannel(BrowserMessageFilter* filter,
                                 IPC::ChannelProxy* channel) {
    filter->RegisterAssociatedInterfaces(channel);
    channel->AddFilter(filter->GetFilter());
  }
};

class ProxyRunner : public IPC::Listener {
 public:
  ProxyRunner(mojo::ScopedMessagePipeHandle pipe,
              bool for_server,
              scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner) {
    std::unique_ptr<IPC::ChannelFactory> factory;
    if (for_server) {
      factory = IPC::ChannelMojo::CreateServerFactory(
          std::move(pipe), ipc_task_runner);
    } else {
      factory = IPC::ChannelMojo::CreateClientFactory(
          std::move(pipe), ipc_task_runner);
    }
    channel_ = IPC::ChannelProxy::Create(
        std::move(factory), this, ipc_task_runner);
  }

  void ShutDown() { channel_.reset(); }

  IPC::ChannelProxy* channel() { return channel_.get(); }

 private:
  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override { return false; }

  std::unique_ptr<IPC::ChannelProxy> channel_;
};

class TestDriverMessageFilter
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<
          mojom::BrowserAssociatedInterfaceTestDriver>,
      public mojom::BrowserAssociatedInterfaceTestDriver {
 public:
  TestDriverMessageFilter()
      : BrowserMessageFilter(0),
        BrowserAssociatedInterface<mojom::BrowserAssociatedInterfaceTestDriver>(
            this, this) {
  }

 private:
  ~TestDriverMessageFilter() override {}

  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    std::string actual_string;
    base::PickleIterator iter(message);
    EXPECT_TRUE(iter.ReadString(&actual_string));
    EXPECT_EQ(next_expected_string_, actual_string);
    message_count_++;
    return true;
  }

  // mojom::BrowserAssociatedInterfaceTestDriver:
  void ExpectString(const std::string& expected) override {
    next_expected_string_ = expected;
  }

  void RequestQuit(const RequestQuitCallback& callback) override {
    EXPECT_EQ(kNumTestMessages, message_count_);
    callback.Run();
    base::MessageLoop::current()->QuitWhenIdle();
  }

  std::string next_expected_string_;
  int message_count_ = 0;
};

class TestClientRunner {
 public:
  explicit TestClientRunner(mojo::ScopedMessagePipeHandle pipe)
      : client_thread_("Test client") {
    client_thread_.Start();
    client_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&RunTestClient, base::Passed(&pipe)));
  }

  ~TestClientRunner() {
    client_thread_.Stop();
    base::RunLoop().RunUntilIdle();
  }

 private:
  static void RunTestClient(mojo::ScopedMessagePipeHandle pipe) {
    base::Thread io_thread("Client IO thread");
    io_thread.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
    ProxyRunner proxy(std::move(pipe), false, io_thread.task_runner());

    mojom::BrowserAssociatedInterfaceTestDriverAssociatedPtr driver;
    proxy.channel()->GetRemoteAssociatedInterface(&driver);

    for (int i = 0; i < kNumTestMessages; ++i) {
      std::string next_message = base::StringPrintf("test %d", i);
      driver->ExpectString(next_message);

      std::unique_ptr<IPC::Message> message(new IPC::Message);
      message->WriteString(next_message);
      proxy.channel()->Send(message.release());
    }

    driver->RequestQuit(base::MessageLoop::QuitWhenIdleClosure());

    base::MessageLoop::ScopedNestableTaskAllower allow_nested_loops(
        base::MessageLoop::current());
    base::RunLoop().Run();

    proxy.ShutDown();
    io_thread.Stop();
    base::RunLoop().RunUntilIdle();
  }

  base::Thread client_thread_;
};

TEST_F(BrowserAssociatedInterfaceTest, Basic) {
  TestBrowserThreadBundle browser_threads_;
  mojo::MessagePipe pipe;
  ProxyRunner proxy(std::move(pipe.handle0), true,
                    BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
  AddFilterToChannel(new TestDriverMessageFilter, proxy.channel());

  TestClientRunner client(std::move(pipe.handle1));
  base::RunLoop().Run();

  proxy.ShutDown();
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
