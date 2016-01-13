// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "build/build_config.h"  // TODO(vtl): Remove this.
#include "mojo/common/test/test_utils.h"
#include "mojo/embedder/platform_channel_pair.h"
#include "mojo/embedder/scoped_platform_handle.h"
#include "mojo/system/channel.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/message_pipe_dispatcher.h"
#include "mojo/system/platform_handle_dispatcher.h"
#include "mojo/system/proxy_message_pipe_endpoint.h"
#include "mojo/system/raw_channel.h"
#include "mojo/system/shared_buffer_dispatcher.h"
#include "mojo/system/test_utils.h"
#include "mojo/system/waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

class RemoteMessagePipeTest : public testing::Test {
 public:
  RemoteMessagePipeTest() : io_thread_(test::TestIOThread::kAutoStart) {}
  virtual ~RemoteMessagePipeTest() {}

  virtual void SetUp() OVERRIDE {
    io_thread_.PostTaskAndWait(
        FROM_HERE,
        base::Bind(&RemoteMessagePipeTest::SetUpOnIOThread,
                   base::Unretained(this)));
  }

  virtual void TearDown() OVERRIDE {
    io_thread_.PostTaskAndWait(
        FROM_HERE,
        base::Bind(&RemoteMessagePipeTest::TearDownOnIOThread,
                   base::Unretained(this)));
  }

 protected:
  // This connects MP 0, port 1 and MP 1, port 0 (leaving MP 0, port 0 and MP 1,
  // port 1 as the user-visible endpoints) to channel 0 and 1, respectively. MP
  // 0, port 1 and MP 1, port 0 must have |ProxyMessagePipeEndpoint|s.
  void ConnectMessagePipes(scoped_refptr<MessagePipe> mp0,
                           scoped_refptr<MessagePipe> mp1) {
    io_thread_.PostTaskAndWait(
        FROM_HERE,
        base::Bind(&RemoteMessagePipeTest::ConnectMessagePipesOnIOThread,
                   base::Unretained(this), mp0, mp1));
  }

  // This connects |mp|'s port |channel_index ^ 1| to channel |channel_index|.
  // It assumes/requires that this is the bootstrap case, i.e., that the
  // endpoint IDs are both/will both be |Channel::kBootstrapEndpointId|. This
  // returns *without* waiting for it to finish connecting.
  void BootstrapMessagePipeNoWait(unsigned channel_index,
                                  scoped_refptr<MessagePipe> mp) {
    io_thread_.PostTask(
        FROM_HERE,
        base::Bind(&RemoteMessagePipeTest::BootstrapMessagePipeOnIOThread,
                   base::Unretained(this), channel_index, mp));
  }

  void RestoreInitialState() {
    io_thread_.PostTaskAndWait(
        FROM_HERE,
        base::Bind(&RemoteMessagePipeTest::RestoreInitialStateOnIOThread,
                   base::Unretained(this)));
  }

  test::TestIOThread* io_thread() { return &io_thread_; }

 private:
  void SetUpOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());

    embedder::PlatformChannelPair channel_pair;
    platform_handles_[0] = channel_pair.PassServerHandle();
    platform_handles_[1] = channel_pair.PassClientHandle();
  }

  void TearDownOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());

    if (channels_[0]) {
      channels_[0]->Shutdown();
      channels_[0] = NULL;
    }
    if (channels_[1]) {
      channels_[1]->Shutdown();
      channels_[1] = NULL;
    }
  }

  void CreateAndInitChannel(unsigned channel_index) {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());
    CHECK(channel_index == 0 || channel_index == 1);
    CHECK(!channels_[channel_index]);

    channels_[channel_index] = new Channel();
    CHECK(channels_[channel_index]->Init(
        RawChannel::Create(platform_handles_[channel_index].Pass())));
  }

  void ConnectMessagePipesOnIOThread(scoped_refptr<MessagePipe> mp0,
                                     scoped_refptr<MessagePipe> mp1) {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());

    if (!channels_[0])
      CreateAndInitChannel(0);
    if (!channels_[1])
      CreateAndInitChannel(1);

    MessageInTransit::EndpointId local_id0 =
        channels_[0]->AttachMessagePipeEndpoint(mp0, 1);
    MessageInTransit::EndpointId local_id1 =
        channels_[1]->AttachMessagePipeEndpoint(mp1, 0);

    CHECK(channels_[0]->RunMessagePipeEndpoint(local_id0, local_id1));
    CHECK(channels_[1]->RunMessagePipeEndpoint(local_id1, local_id0));
  }

  void BootstrapMessagePipeOnIOThread(unsigned channel_index,
                                      scoped_refptr<MessagePipe> mp) {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());
    CHECK(channel_index == 0 || channel_index == 1);

    unsigned port = channel_index ^ 1u;

    CreateAndInitChannel(channel_index);
    MessageInTransit::EndpointId endpoint_id =
        channels_[channel_index]->AttachMessagePipeEndpoint(mp, port);
    if (endpoint_id == MessageInTransit::kInvalidEndpointId)
      return;

    CHECK_EQ(endpoint_id, Channel::kBootstrapEndpointId);
    CHECK(channels_[channel_index]->RunMessagePipeEndpoint(
        Channel::kBootstrapEndpointId, Channel::kBootstrapEndpointId));
  }

  void RestoreInitialStateOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());

    TearDownOnIOThread();
    SetUpOnIOThread();
  }

  test::TestIOThread io_thread_;
  embedder::ScopedPlatformHandle platform_handles_[2];
  scoped_refptr<Channel> channels_[2];

  DISALLOW_COPY_AND_ASSIGN(RemoteMessagePipeTest);
};

TEST_F(RemoteMessagePipeTest, Basic) {
  static const char kHello[] = "hello";
  static const char kWorld[] = "world!!!1!!!1!";
  char buffer[100] = { 0 };
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  Waiter waiter;
  uint32_t context = 0;

  // Connect message pipes. MP 0, port 1 will be attached to channel 0 and
  // connected to MP 1, port 0, which will be attached to channel 1. This leaves
  // MP 0, port 0 and MP 1, port 1 as the "user-facing" endpoints.

  scoped_refptr<MessagePipe> mp0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp0, mp1);

  // Write in one direction: MP 0, port 0 -> ... -> MP 1, port 1.

  // Prepare to wait on MP 1, port 1. (Add the waiter now. Otherwise, if we do
  // it later, it might already be readable.)
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->AddWaiter(1, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 123));

  // Write to MP 0, port 0.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp0->WriteMessage(0,
                              kHello, sizeof(kHello),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait.
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_EQ(123u, context);
  mp1->RemoveWaiter(1, &waiter);

  // Read from MP 1, port 1.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kHello), static_cast<size_t>(buffer_size));
  EXPECT_STREQ(kHello, buffer);

  // Write in the other direction: MP 1, port 1 -> ... -> MP 0, port 0.

  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp0->AddWaiter(0, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 456));

  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->WriteMessage(1,
                              kWorld, sizeof(kWorld),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_EQ(456u, context);
  mp0->RemoveWaiter(0, &waiter);

  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            mp0->ReadMessage(0,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kWorld), static_cast<size_t>(buffer_size));
  EXPECT_STREQ(kWorld, buffer);

  // Close MP 0, port 0.
  mp0->Close(0);

  // Try to wait for MP 1, port 1 to become readable. This will eventually fail
  // when it realizes that MP 0, port 0 has been closed. (It may also fail
  // immediately.)
  waiter.Init();
  MojoResult result =
      mp1->AddWaiter(1, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 789);
  if (result == MOJO_RESULT_OK) {
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              waiter.Wait(MOJO_DEADLINE_INDEFINITE, &context));
    EXPECT_EQ(789u, context);
    mp1->RemoveWaiter(1, &waiter);
  } else {
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
  }

  // And MP 1, port 1.
  mp1->Close(1);
}

TEST_F(RemoteMessagePipeTest, Multiplex) {
  static const char kHello[] = "hello";
  static const char kWorld[] = "world!!!1!!!1!";
  char buffer[100] = { 0 };
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  Waiter waiter;
  uint32_t context = 0;

  // Connect message pipes as in the |Basic| test.

  scoped_refptr<MessagePipe> mp0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp0, mp1);

  // Now put another message pipe on the channel.

  scoped_refptr<MessagePipe> mp2(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp3(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp2, mp3);

  // Write: MP 2, port 0 -> MP 3, port 1.

  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp3->AddWaiter(1, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 789));

  EXPECT_EQ(MOJO_RESULT_OK,
            mp2->WriteMessage(0,
                              kHello, sizeof(kHello),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_EQ(789u, context);
  mp3->RemoveWaiter(1, &waiter);

  // Make sure there's nothing on MP 0, port 0 or MP 1, port 1 or MP 2, port 0.
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp0->ReadMessage(0,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp1->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp2->ReadMessage(0,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));

  // Read from MP 3, port 1.
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            mp3->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kHello), static_cast<size_t>(buffer_size));
  EXPECT_STREQ(kHello, buffer);

  // Write: MP 0, port 0 -> MP 1, port 1 again.

  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->AddWaiter(1, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 123));

  EXPECT_EQ(MOJO_RESULT_OK,
            mp0->WriteMessage(0,
                              kWorld, sizeof(kWorld),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_EQ(123u, context);
  mp1->RemoveWaiter(1, &waiter);

  // Make sure there's nothing on the other ports.
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp0->ReadMessage(0,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp2->ReadMessage(0,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp3->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));

  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kWorld), static_cast<size_t>(buffer_size));
  EXPECT_STREQ(kWorld, buffer);

  mp0->Close(0);
  mp1->Close(1);
  mp2->Close(0);
  mp3->Close(1);
}

TEST_F(RemoteMessagePipeTest, CloseBeforeConnect) {
  static const char kHello[] = "hello";
  char buffer[100] = { 0 };
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  Waiter waiter;
  uint32_t context = 0;

  // Connect message pipes. MP 0, port 1 will be attached to channel 0 and
  // connected to MP 1, port 0, which will be attached to channel 1. This leaves
  // MP 0, port 0 and MP 1, port 1 as the "user-facing" endpoints.

  scoped_refptr<MessagePipe> mp0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));

  // Write to MP 0, port 0.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp0->WriteMessage(0,
                              kHello, sizeof(kHello),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

  BootstrapMessagePipeNoWait(0, mp0);


  // Close MP 0, port 0 before channel 1 is even connected.
  mp0->Close(0);

  scoped_refptr<MessagePipe> mp1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));

  // Prepare to wait on MP 1, port 1. (Add the waiter now. Otherwise, if we do
  // it later, it might already be readable.)
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->AddWaiter(1, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 123));

  BootstrapMessagePipeNoWait(1, mp1);

  // Wait.
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_EQ(123u, context);
  mp1->RemoveWaiter(1, &waiter);

  // Read from MP 1, port 1.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kHello), static_cast<size_t>(buffer_size));
  EXPECT_STREQ(kHello, buffer);

  // And MP 1, port 1.
  mp1->Close(1);
}

TEST_F(RemoteMessagePipeTest, HandlePassing) {
  static const char kHello[] = "hello";
  Waiter waiter;
  uint32_t context = 0;

  scoped_refptr<MessagePipe> mp0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp0, mp1);

  // We'll try to pass this dispatcher.
  scoped_refptr<MessagePipeDispatcher> dispatcher(new MessagePipeDispatcher(
      MessagePipeDispatcher::kDefaultCreateOptions));
  scoped_refptr<MessagePipe> local_mp(new MessagePipe());
  dispatcher->Init(local_mp, 0);

  // Prepare to wait on MP 1, port 1. (Add the waiter now. Otherwise, if we do
  // it later, it might already be readable.)
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->AddWaiter(1, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 123));

  // Write to MP 0, port 0.
  {
    DispatcherTransport
        transport(test::DispatcherTryStartTransport(dispatcher.get()));
    EXPECT_TRUE(transport.is_valid());

    std::vector<DispatcherTransport> transports;
    transports.push_back(transport);
    EXPECT_EQ(MOJO_RESULT_OK,
              mp0->WriteMessage(0, kHello, sizeof(kHello), &transports,
                                MOJO_WRITE_MESSAGE_FLAG_NONE));
    transport.End();

    // |dispatcher| should have been closed. This is |DCHECK()|ed when the
    // |dispatcher| is destroyed.
    EXPECT_TRUE(dispatcher->HasOneRef());
    dispatcher = NULL;
  }

  // Wait.
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_EQ(123u, context);
  mp1->RemoveWaiter(1, &waiter);

  // Read from MP 1, port 1.
  char read_buffer[100] = { 0 };
  uint32_t read_buffer_size = static_cast<uint32_t>(sizeof(read_buffer));
  DispatcherVector read_dispatchers;
  uint32_t read_num_dispatchers = 10;  // Maximum to get.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->ReadMessage(1, read_buffer, &read_buffer_size,
                             &read_dispatchers, &read_num_dispatchers,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kHello), static_cast<size_t>(read_buffer_size));
  EXPECT_STREQ(kHello, read_buffer);
  EXPECT_EQ(1u, read_dispatchers.size());
  EXPECT_EQ(1u, read_num_dispatchers);
  ASSERT_TRUE(read_dispatchers[0]);
  EXPECT_TRUE(read_dispatchers[0]->HasOneRef());

  EXPECT_EQ(Dispatcher::kTypeMessagePipe, read_dispatchers[0]->GetType());
  dispatcher = static_cast<MessagePipeDispatcher*>(read_dispatchers[0].get());

  // Write to "local_mp", port 1.
  EXPECT_EQ(MOJO_RESULT_OK,
            local_mp->WriteMessage(1, kHello, sizeof(kHello), NULL,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));

  // TODO(vtl): FIXME -- We (racily) crash if I close |dispatcher| immediately
  // here. (We don't crash if I sleep and then close.)

  // Wait for the dispatcher to become readable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher->AddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 456));
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_EQ(456u, context);
  dispatcher->RemoveWaiter(&waiter);

  // Read from the dispatcher.
  memset(read_buffer, 0, sizeof(read_buffer));
  read_buffer_size = static_cast<uint32_t>(sizeof(read_buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher->ReadMessage(read_buffer, &read_buffer_size, 0, NULL,
                                    MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kHello), static_cast<size_t>(read_buffer_size));
  EXPECT_STREQ(kHello, read_buffer);

  // Prepare to wait on "local_mp", port 1.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            local_mp->AddWaiter(1, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 789));

  // Write to the dispatcher.
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher->WriteMessage(kHello, sizeof(kHello), NULL,
                                     MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait.
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_EQ(789u, context);
  local_mp->RemoveWaiter(1, &waiter);

  // Read from "local_mp", port 1.
  memset(read_buffer, 0, sizeof(read_buffer));
  read_buffer_size = static_cast<uint32_t>(sizeof(read_buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            local_mp->ReadMessage(1, read_buffer, &read_buffer_size, NULL, NULL,
                                  MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kHello), static_cast<size_t>(read_buffer_size));
  EXPECT_STREQ(kHello, read_buffer);

  // TODO(vtl): Also test that messages queued up before the handle was sent are
  // delivered properly.

  // Close everything that belongs to us.
  mp0->Close(0);
  mp1->Close(1);
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());
  // Note that |local_mp|'s port 0 belong to |dispatcher|, which was closed.
  local_mp->Close(1);
}

#if defined(OS_POSIX)
#define MAYBE_SharedBufferPassing SharedBufferPassing
#else
// Not yet implemented (on Windows).
#define MAYBE_SharedBufferPassing DISABLED_SharedBufferPassing
#endif
TEST_F(RemoteMessagePipeTest, MAYBE_SharedBufferPassing) {
  static const char kHello[] = "hello";
  Waiter waiter;
  uint32_t context = 0;

  scoped_refptr<MessagePipe> mp0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp0, mp1);

  // We'll try to pass this dispatcher.
  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::Create(
                SharedBufferDispatcher::kDefaultCreateOptions, 100,
                &dispatcher));
  ASSERT_TRUE(dispatcher);

  // Make a mapping.
  scoped_ptr<RawSharedBufferMapping> mapping0;
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher->MapBuffer(0, 100, MOJO_MAP_BUFFER_FLAG_NONE,
                                  &mapping0));
  ASSERT_TRUE(mapping0);
  ASSERT_TRUE(mapping0->base());
  ASSERT_EQ(100u, mapping0->length());
  static_cast<char*>(mapping0->base())[0] = 'A';
  static_cast<char*>(mapping0->base())[50] = 'B';
  static_cast<char*>(mapping0->base())[99] = 'C';

  // Prepare to wait on MP 1, port 1. (Add the waiter now. Otherwise, if we do
  // it later, it might already be readable.)
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->AddWaiter(1, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 123));

  // Write to MP 0, port 0.
  {
    DispatcherTransport
        transport(test::DispatcherTryStartTransport(dispatcher.get()));
    EXPECT_TRUE(transport.is_valid());

    std::vector<DispatcherTransport> transports;
    transports.push_back(transport);
    EXPECT_EQ(MOJO_RESULT_OK,
              mp0->WriteMessage(0, kHello, sizeof(kHello), &transports,
                                MOJO_WRITE_MESSAGE_FLAG_NONE));
    transport.End();

    // |dispatcher| should have been closed. This is |DCHECK()|ed when the
    // |dispatcher| is destroyed.
    EXPECT_TRUE(dispatcher->HasOneRef());
    dispatcher = NULL;
  }

  // Wait.
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_EQ(123u, context);
  mp1->RemoveWaiter(1, &waiter);

  // Read from MP 1, port 1.
  char read_buffer[100] = { 0 };
  uint32_t read_buffer_size = static_cast<uint32_t>(sizeof(read_buffer));
  DispatcherVector read_dispatchers;
  uint32_t read_num_dispatchers = 10;  // Maximum to get.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->ReadMessage(1, read_buffer, &read_buffer_size,
                             &read_dispatchers, &read_num_dispatchers,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kHello), static_cast<size_t>(read_buffer_size));
  EXPECT_STREQ(kHello, read_buffer);
  EXPECT_EQ(1u, read_dispatchers.size());
  EXPECT_EQ(1u, read_num_dispatchers);
  ASSERT_TRUE(read_dispatchers[0]);
  EXPECT_TRUE(read_dispatchers[0]->HasOneRef());

  EXPECT_EQ(Dispatcher::kTypeSharedBuffer, read_dispatchers[0]->GetType());
  dispatcher =
      static_cast<SharedBufferDispatcher*>(read_dispatchers[0].get());

  // Make another mapping.
  scoped_ptr<RawSharedBufferMapping> mapping1;
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher->MapBuffer(0, 100, MOJO_MAP_BUFFER_FLAG_NONE,
                                  &mapping1));
  ASSERT_TRUE(mapping1);
  ASSERT_TRUE(mapping1->base());
  ASSERT_EQ(100u, mapping1->length());
  EXPECT_NE(mapping1->base(), mapping0->base());
  EXPECT_EQ('A', static_cast<char*>(mapping1->base())[0]);
  EXPECT_EQ('B', static_cast<char*>(mapping1->base())[50]);
  EXPECT_EQ('C', static_cast<char*>(mapping1->base())[99]);

  // Write stuff either way.
  static_cast<char*>(mapping1->base())[1] = 'x';
  EXPECT_EQ('x', static_cast<char*>(mapping0->base())[1]);
  static_cast<char*>(mapping0->base())[2] = 'y';
  EXPECT_EQ('y', static_cast<char*>(mapping1->base())[2]);

  // Kill the first mapping; the second should still be valid.
  mapping0.reset();
  EXPECT_EQ('A', static_cast<char*>(mapping1->base())[0]);

  // Close everything that belongs to us.
  mp0->Close(0);
  mp1->Close(1);
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());

  // The second mapping should still be good.
  EXPECT_EQ('x', static_cast<char*>(mapping1->base())[1]);
}

#if defined(OS_POSIX)
#define MAYBE_PlatformHandlePassing PlatformHandlePassing
#else
// Not yet implemented (on Windows).
#define MAYBE_PlatformHandlePassing DISABLED_PlatformHandlePassing
#endif
TEST_F(RemoteMessagePipeTest, MAYBE_PlatformHandlePassing) {
  static const char kHello[] = "hello";
  static const char kWorld[] = "world";
  Waiter waiter;
  uint32_t context = 0;

  scoped_refptr<MessagePipe> mp0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp0, mp1);

  base::FilePath unused;
  base::ScopedFILE fp(CreateAndOpenTemporaryFile(&unused));
  EXPECT_EQ(sizeof(kHello), fwrite(kHello, 1, sizeof(kHello), fp.get()));
  // We'll try to pass this dispatcher, which will cause a |PlatformHandle| to
  // be passed.
  scoped_refptr<PlatformHandleDispatcher> dispatcher(
      new PlatformHandleDispatcher(
          mojo::test::PlatformHandleFromFILE(fp.Pass())));

  // Prepare to wait on MP 1, port 1. (Add the waiter now. Otherwise, if we do
  // it later, it might already be readable.)
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->AddWaiter(1, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 123));

  // Write to MP 0, port 0.
  {
    DispatcherTransport
        transport(test::DispatcherTryStartTransport(dispatcher.get()));
    EXPECT_TRUE(transport.is_valid());

    std::vector<DispatcherTransport> transports;
    transports.push_back(transport);
    EXPECT_EQ(MOJO_RESULT_OK,
              mp0->WriteMessage(0, kWorld, sizeof(kWorld), &transports,
                                MOJO_WRITE_MESSAGE_FLAG_NONE));
    transport.End();

    // |dispatcher| should have been closed. This is |DCHECK()|ed when the
    // |dispatcher| is destroyed.
    EXPECT_TRUE(dispatcher->HasOneRef());
    dispatcher = NULL;
  }

  // Wait.
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_EQ(123u, context);
  mp1->RemoveWaiter(1, &waiter);

  // Read from MP 1, port 1.
  char read_buffer[100] = { 0 };
  uint32_t read_buffer_size = static_cast<uint32_t>(sizeof(read_buffer));
  DispatcherVector read_dispatchers;
  uint32_t read_num_dispatchers = 10;  // Maximum to get.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->ReadMessage(1, read_buffer, &read_buffer_size,
                             &read_dispatchers, &read_num_dispatchers,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(kWorld), static_cast<size_t>(read_buffer_size));
  EXPECT_STREQ(kWorld, read_buffer);
  EXPECT_EQ(1u, read_dispatchers.size());
  EXPECT_EQ(1u, read_num_dispatchers);
  ASSERT_TRUE(read_dispatchers[0]);
  EXPECT_TRUE(read_dispatchers[0]->HasOneRef());

  EXPECT_EQ(Dispatcher::kTypePlatformHandle, read_dispatchers[0]->GetType());
  dispatcher =
      static_cast<PlatformHandleDispatcher*>(read_dispatchers[0].get());

  embedder::ScopedPlatformHandle h = dispatcher->PassPlatformHandle().Pass();
  EXPECT_TRUE(h.is_valid());

  fp = mojo::test::FILEFromPlatformHandle(h.Pass(), "rb").Pass();
  EXPECT_FALSE(h.is_valid());
  EXPECT_TRUE(fp);

  rewind(fp.get());
  memset(read_buffer, 0, sizeof(read_buffer));
  EXPECT_EQ(sizeof(kHello),
            fread(read_buffer, 1, sizeof(read_buffer), fp.get()));
  EXPECT_STREQ(kHello, read_buffer);

  // Close everything that belongs to us.
  mp0->Close(0);
  mp1->Close(1);
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());
}

// Test racing closes (on each end).
// Note: A flaky failure would almost certainly indicate a problem in the code
// itself (not in the test). Also, any logged warnings/errors would also
// probably be indicative of bugs.
TEST_F(RemoteMessagePipeTest, RacingClosesStress) {
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(5);

  for (unsigned i = 0; i < 256; i++) {
    DVLOG(2) << "---------------------------------------- " << i;
    scoped_refptr<MessagePipe> mp0(new MessagePipe(
        scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
        scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
    BootstrapMessagePipeNoWait(0, mp0);

    scoped_refptr<MessagePipe> mp1(new MessagePipe(
        scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
        scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
    BootstrapMessagePipeNoWait(1, mp1);

    if (i & 1u) {
      io_thread()->task_runner()->PostTask(
          FROM_HERE, base::Bind(&base::PlatformThread::Sleep, delay));
    }
    if (i & 2u)
      base::PlatformThread::Sleep(delay);

    mp0->Close(0);

    if (i & 4u) {
      io_thread()->task_runner()->PostTask(
          FROM_HERE, base::Bind(&base::PlatformThread::Sleep, delay));
    }
    if (i & 8u)
      base::PlatformThread::Sleep(delay);

    mp1->Close(1);

    RestoreInitialState();
  }
}

}  // namespace
}  // namespace system
}  // namespace mojo
