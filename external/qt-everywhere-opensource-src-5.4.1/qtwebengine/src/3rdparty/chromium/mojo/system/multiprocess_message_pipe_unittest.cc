// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "build/build_config.h"  // TODO(vtl): Remove this.
#include "mojo/common/test/multiprocess_test_helper.h"
#include "mojo/common/test/test_utils.h"
#include "mojo/embedder/scoped_platform_handle.h"
#include "mojo/system/channel.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/platform_handle_dispatcher.h"
#include "mojo/system/proxy_message_pipe_endpoint.h"
#include "mojo/system/raw_channel.h"
#include "mojo/system/raw_shared_buffer.h"
#include "mojo/system/shared_buffer_dispatcher.h"
#include "mojo/system/test_utils.h"
#include "mojo/system/waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

class ChannelThread {
 public:
  ChannelThread() : test_io_thread_(test::TestIOThread::kManualStart) {}
  ~ChannelThread() {
    Stop();
  }

  void Start(embedder::ScopedPlatformHandle platform_handle,
             scoped_refptr<MessagePipe> message_pipe) {
    test_io_thread_.Start();
    test_io_thread_.PostTaskAndWait(
        FROM_HERE,
        base::Bind(&ChannelThread::InitChannelOnIOThread,
                   base::Unretained(this), base::Passed(&platform_handle),
                   message_pipe));
  }

  void Stop() {
    if (channel_) {
      // Hack to flush write buffers before quitting.
      // TODO(vtl): Remove this once |Channel| has a
      // |FlushWriteBufferAndShutdown()| (or whatever).
      while (!channel_->IsWriteBufferEmpty())
        base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(20));

      test_io_thread_.PostTaskAndWait(
          FROM_HERE,
          base::Bind(&ChannelThread::ShutdownChannelOnIOThread,
                     base::Unretained(this)));
    }
    test_io_thread_.Stop();
  }

 private:
  void InitChannelOnIOThread(embedder::ScopedPlatformHandle platform_handle,
                             scoped_refptr<MessagePipe> message_pipe) {
    CHECK_EQ(base::MessageLoop::current(), test_io_thread_.message_loop());
    CHECK(platform_handle.is_valid());

    // Create and initialize |Channel|.
    channel_ = new Channel();
    CHECK(channel_->Init(RawChannel::Create(platform_handle.Pass())));

    // Attach the message pipe endpoint.
    // Note: On the "server" (parent process) side, we need not attach the
    // message pipe endpoint immediately. However, on the "client" (child
    // process) side, this *must* be done here -- otherwise, the |Channel| may
    // receive/process messages (which it can do as soon as it's hooked up to
    // the IO thread message loop, and that message loop runs) before the
    // message pipe endpoint is attached.
    CHECK_EQ(channel_->AttachMessagePipeEndpoint(message_pipe, 1),
             Channel::kBootstrapEndpointId);
    CHECK(channel_->RunMessagePipeEndpoint(Channel::kBootstrapEndpointId,
                                           Channel::kBootstrapEndpointId));
  }

  void ShutdownChannelOnIOThread() {
    CHECK(channel_);
    channel_->Shutdown();
    channel_ = NULL;
  }

  test::TestIOThread test_io_thread_;
  scoped_refptr<Channel> channel_;

  DISALLOW_COPY_AND_ASSIGN(ChannelThread);
};

class MultiprocessMessagePipeTest : public testing::Test {
 public:
  MultiprocessMessagePipeTest() {}
  virtual ~MultiprocessMessagePipeTest() {}

 protected:
  void Init(scoped_refptr<MessagePipe> mp) {
    channel_thread_.Start(helper_.server_platform_handle.Pass(), mp);
  }

  mojo::test::MultiprocessTestHelper* helper() { return &helper_; }

 private:
  ChannelThread channel_thread_;
  mojo::test::MultiprocessTestHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(MultiprocessMessagePipeTest);
};

MojoResult WaitIfNecessary(scoped_refptr<MessagePipe> mp,
                           MojoHandleSignals signals) {
  Waiter waiter;
  waiter.Init();

  MojoResult add_result = mp->AddWaiter(0, &waiter, signals, 0);
  if (add_result != MOJO_RESULT_OK) {
    return (add_result == MOJO_RESULT_ALREADY_EXISTS) ? MOJO_RESULT_OK :
                                                        add_result;
  }

  MojoResult wait_result = waiter.Wait(MOJO_DEADLINE_INDEFINITE, NULL);
  mp->RemoveWaiter(0, &waiter);
  return wait_result;
}

// For each message received, sends a reply message with the same contents
// repeated twice, until the other end is closed or it receives "quitquitquit"
// (which it doesn't reply to). It'll return the number of messages received,
// not including any "quitquitquit" message, modulo 100.
MOJO_MULTIPROCESS_TEST_CHILD_MAIN(EchoEcho) {
  ChannelThread channel_thread;
  embedder::ScopedPlatformHandle client_platform_handle =
      mojo::test::MultiprocessTestHelper::client_platform_handle.Pass();
  CHECK(client_platform_handle.is_valid());
  scoped_refptr<MessagePipe> mp(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  channel_thread.Start(client_platform_handle.Pass(), mp);

  const std::string quitquitquit("quitquitquit");
  int rv = 0;
  for (;; rv = (rv + 1) % 100) {
    // Wait for our end of the message pipe to be readable.
    MojoResult result = WaitIfNecessary(mp, MOJO_HANDLE_SIGNAL_READABLE);
    if (result != MOJO_RESULT_OK) {
      // It was closed, probably.
      CHECK_EQ(result, MOJO_RESULT_FAILED_PRECONDITION);
      break;
    }

    std::string read_buffer(1000, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(mp->ReadMessage(0,
                             &read_buffer[0], &read_buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    VLOG(2) << "Child got: " << read_buffer;

    if (read_buffer == quitquitquit) {
      VLOG(2) << "Child quitting.";
      break;
    }

    std::string write_buffer = read_buffer + read_buffer;
    CHECK_EQ(mp->WriteMessage(0,
                              write_buffer.data(),
                              static_cast<uint32_t>(write_buffer.size()),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
  }

  mp->Close(0);
  return rv;
}

// Sends "hello" to child, and expects "hellohello" back.
TEST_F(MultiprocessMessagePipeTest, Basic) {
  helper()->StartChild("EchoEcho");

  scoped_refptr<MessagePipe> mp(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  Init(mp);

  std::string hello("hello");
  EXPECT_EQ(MOJO_RESULT_OK,
            mp->WriteMessage(0,
                             hello.data(), static_cast<uint32_t>(hello.size()),
                             NULL,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(MOJO_RESULT_OK, WaitIfNecessary(mp, MOJO_HANDLE_SIGNAL_READABLE));

  std::string read_buffer(1000, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(mp->ReadMessage(0,
                           &read_buffer[0], &read_buffer_size,
                           NULL, NULL,
                           MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  VLOG(2) << "Parent got: " << read_buffer;
  EXPECT_EQ(hello + hello, read_buffer);

  mp->Close(0);

  // We sent one message.
  EXPECT_EQ(1 % 100, helper()->WaitForChildShutdown());
}

// Sends a bunch of messages to the child. Expects them "repeated" back. Waits
// for the child to close its end before quitting.
TEST_F(MultiprocessMessagePipeTest, QueueMessages) {
  helper()->StartChild("EchoEcho");

  scoped_refptr<MessagePipe> mp(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  Init(mp);

  static const size_t kNumMessages = 1001;
  for (size_t i = 0; i < kNumMessages; i++) {
    std::string write_buffer(i, 'A' + (i % 26));
    EXPECT_EQ(MOJO_RESULT_OK,
              mp->WriteMessage(0,
                               write_buffer.data(),
                               static_cast<uint32_t>(write_buffer.size()),
                               NULL,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));
  }

  const std::string quitquitquit("quitquitquit");
  EXPECT_EQ(MOJO_RESULT_OK,
            mp->WriteMessage(0,
                             quitquitquit.data(),
                             static_cast<uint32_t>(quitquitquit.size()),
                             NULL,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  for (size_t i = 0; i < kNumMessages; i++) {
    EXPECT_EQ(MOJO_RESULT_OK, WaitIfNecessary(mp, MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(kNumMessages * 2, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(mp->ReadMessage(0,
                             &read_buffer[0], &read_buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);

    EXPECT_EQ(std::string(i * 2, 'A' + (i % 26)), read_buffer);
  }

  // Wait for it to become readable, which should fail (since we sent
  // "quitquitquit").
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WaitIfNecessary(mp, MOJO_HANDLE_SIGNAL_READABLE));

  mp->Close(0);

  EXPECT_EQ(static_cast<int>(kNumMessages % 100),
            helper()->WaitForChildShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(CheckSharedBuffer) {
  ChannelThread channel_thread;
  embedder::ScopedPlatformHandle client_platform_handle =
      mojo::test::MultiprocessTestHelper::client_platform_handle.Pass();
  CHECK(client_platform_handle.is_valid());
  scoped_refptr<MessagePipe> mp(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  channel_thread.Start(client_platform_handle.Pass(), mp);

  // Wait for the first message from our parent.
  CHECK_EQ(WaitIfNecessary(mp, MOJO_HANDLE_SIGNAL_READABLE), MOJO_RESULT_OK);

  // It should have a shared buffer.
  std::string read_buffer(100, '\0');
  uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
  DispatcherVector dispatchers;
  uint32_t num_dispatchers = 10;  // Maximum number to receive.
  CHECK_EQ(mp->ReadMessage(0,
                           &read_buffer[0], &num_bytes,
                           &dispatchers, &num_dispatchers,
                           MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(num_bytes);
  CHECK_EQ(read_buffer, std::string("go 1"));
  CHECK_EQ(num_dispatchers, 1u);

  CHECK_EQ(dispatchers[0]->GetType(), Dispatcher::kTypeSharedBuffer);

  scoped_refptr<SharedBufferDispatcher> dispatcher(
      static_cast<SharedBufferDispatcher*>(dispatchers[0].get()));

  // Make a mapping.
  scoped_ptr<RawSharedBufferMapping> mapping;
  CHECK_EQ(dispatcher->MapBuffer(0, 100, MOJO_MAP_BUFFER_FLAG_NONE, &mapping),
           MOJO_RESULT_OK);
  CHECK(mapping);
  CHECK(mapping->base());
  CHECK_EQ(mapping->length(), 100u);

  // Write some stuff to the shared buffer.
  static const char kHello[] = "hello";
  memcpy(mapping->base(), kHello, sizeof(kHello));

  // We should be able to close the dispatcher now.
  dispatcher->Close();

  // And send a message to signal that we've written stuff.
  const std::string go2("go 2");
  CHECK_EQ(mp->WriteMessage(0,
                            &go2[0],
                            static_cast<uint32_t>(go2.size()),
                            NULL,
                            MOJO_WRITE_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);

  // Now wait for our parent to send us a message.
  CHECK_EQ(WaitIfNecessary(mp, MOJO_HANDLE_SIGNAL_READABLE), MOJO_RESULT_OK);

  read_buffer = std::string(100, '\0');
  num_bytes = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(mp->ReadMessage(0,
                           &read_buffer[0], &num_bytes,
                           NULL, NULL,
                           MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(num_bytes);
  CHECK_EQ(read_buffer, std::string("go 3"));

  // It should have written something to the shared buffer.
  static const char kWorld[] = "world!!!";
  CHECK_EQ(memcmp(mapping->base(), kWorld, sizeof(kWorld)), 0);

  // And we're done.
  mp->Close(0);

  return 0;
}

#if defined(OS_POSIX)
#define MAYBE_SharedBufferPassing SharedBufferPassing
#else
// Not yet implemented (on Windows).
#define MAYBE_SharedBufferPassing DISABLED_SharedBufferPassing
#endif
TEST_F(MultiprocessMessagePipeTest, MAYBE_SharedBufferPassing) {
  helper()->StartChild("CheckSharedBuffer");

  scoped_refptr<MessagePipe> mp(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  Init(mp);

  // Make a shared buffer.
  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::Create(
                SharedBufferDispatcher::kDefaultCreateOptions, 100,
                &dispatcher));
  ASSERT_TRUE(dispatcher);

  // Make a mapping.
  scoped_ptr<RawSharedBufferMapping> mapping;
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher->MapBuffer(0, 100, MOJO_MAP_BUFFER_FLAG_NONE, &mapping));
  ASSERT_TRUE(mapping);
  ASSERT_TRUE(mapping->base());
  ASSERT_EQ(100u, mapping->length());

  // Send the shared buffer.
  const std::string go1("go 1");
  DispatcherTransport transport(
      test::DispatcherTryStartTransport(dispatcher.get()));
  ASSERT_TRUE(transport.is_valid());

  std::vector<DispatcherTransport> transports;
  transports.push_back(transport);
  EXPECT_EQ(MOJO_RESULT_OK,
            mp->WriteMessage(0,
                             &go1[0],
                             static_cast<uint32_t>(go1.size()),
                             &transports,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
  transport.End();

  EXPECT_TRUE(dispatcher->HasOneRef());
  dispatcher = NULL;

  // Wait for a message from the child.
  EXPECT_EQ(MOJO_RESULT_OK, WaitIfNecessary(mp, MOJO_HANDLE_SIGNAL_READABLE));

  std::string read_buffer(100, '\0');
  uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
  EXPECT_EQ(MOJO_RESULT_OK,
            mp->ReadMessage(0,
                            &read_buffer[0], &num_bytes,
                            NULL, NULL,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  read_buffer.resize(num_bytes);
  EXPECT_EQ(std::string("go 2"), read_buffer);

  // After we get it, the child should have written something to the shared
  // buffer.
  static const char kHello[] = "hello";
  EXPECT_EQ(0, memcmp(mapping->base(), kHello, sizeof(kHello)));

  // Now we'll write some stuff to the shared buffer.
  static const char kWorld[] = "world!!!";
  memcpy(mapping->base(), kWorld, sizeof(kWorld));

  // And send a message to signal that we've written stuff.
  const std::string go3("go 3");
  EXPECT_EQ(MOJO_RESULT_OK,
            mp->WriteMessage(0,
                             &go3[0],
                             static_cast<uint32_t>(go3.size()),
                             NULL,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for |mp| to become readable, which should fail.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WaitIfNecessary(mp, MOJO_HANDLE_SIGNAL_READABLE));

  mp->Close(0);

  EXPECT_EQ(0, helper()->WaitForChildShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(CheckPlatformHandleFile) {
  ChannelThread channel_thread;
  embedder::ScopedPlatformHandle client_platform_handle =
      mojo::test::MultiprocessTestHelper::client_platform_handle.Pass();
  CHECK(client_platform_handle.is_valid());
  scoped_refptr<MessagePipe> mp(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  channel_thread.Start(client_platform_handle.Pass(), mp);

  CHECK_EQ(WaitIfNecessary(mp, MOJO_HANDLE_SIGNAL_READABLE), MOJO_RESULT_OK);

  std::string read_buffer(100, '\0');
  uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
  DispatcherVector dispatchers;
  uint32_t num_dispatchers = 10;  // Maximum number to receive.
  CHECK_EQ(mp->ReadMessage(0,
                           &read_buffer[0], &num_bytes,
                           &dispatchers, &num_dispatchers,
                           MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  mp->Close(0);

  read_buffer.resize(num_bytes);
  CHECK_EQ(read_buffer, std::string("hello"));
  CHECK_EQ(num_dispatchers, 1u);

  CHECK_EQ(dispatchers[0]->GetType(), Dispatcher::kTypePlatformHandle);

  scoped_refptr<PlatformHandleDispatcher> dispatcher(
      static_cast<PlatformHandleDispatcher*>(dispatchers[0].get()));
  embedder::ScopedPlatformHandle h = dispatcher->PassPlatformHandle().Pass();
  CHECK(h.is_valid());
  dispatcher->Close();

  base::ScopedFILE fp(mojo::test::FILEFromPlatformHandle(h.Pass(), "r"));
  CHECK(fp);
  std::string fread_buffer(100, '\0');
  size_t bytes_read = fread(&fread_buffer[0], 1, fread_buffer.size(), fp.get());
  fread_buffer.resize(bytes_read);
  CHECK_EQ(fread_buffer, "world");

  return 0;
}

#if defined(OS_POSIX)
#define MAYBE_PlatformHandlePassing PlatformHandlePassing
#else
// Not yet implemented (on Windows).
#define MAYBE_PlatformHandlePassing DISABLED_PlatformHandlePassing
#endif
TEST_F(MultiprocessMessagePipeTest, MAYBE_PlatformHandlePassing) {
  helper()->StartChild("CheckPlatformHandleFile");

  scoped_refptr<MessagePipe> mp(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  Init(mp);

  base::FilePath unused;
  base::ScopedFILE fp(CreateAndOpenTemporaryFile(&unused));
  const std::string world("world");
  ASSERT_EQ(fwrite(&world[0], 1, world.size(), fp.get()), world.size());
  fflush(fp.get());
  rewind(fp.get());

  embedder::ScopedPlatformHandle h(
      mojo::test::PlatformHandleFromFILE(fp.Pass()));
  scoped_refptr<PlatformHandleDispatcher> dispatcher(
      new PlatformHandleDispatcher(h.Pass()));

  const std::string hello("hello");
  DispatcherTransport transport(
      test::DispatcherTryStartTransport(dispatcher.get()));
  ASSERT_TRUE(transport.is_valid());

  std::vector<DispatcherTransport> transports;
  transports.push_back(transport);
  EXPECT_EQ(MOJO_RESULT_OK,
            mp->WriteMessage(0,
                             &hello[0],
                             static_cast<uint32_t>(hello.size()),
                             &transports,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
  transport.End();

  EXPECT_TRUE(dispatcher->HasOneRef());
  dispatcher = NULL;

  // Wait for it to become readable, which should fail.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WaitIfNecessary(mp, MOJO_HANDLE_SIGNAL_READABLE));

  mp->Close(0);

  EXPECT_EQ(0, helper()->WaitForChildShutdown());
}

}  // namespace
}  // namespace system
}  // namespace mojo
