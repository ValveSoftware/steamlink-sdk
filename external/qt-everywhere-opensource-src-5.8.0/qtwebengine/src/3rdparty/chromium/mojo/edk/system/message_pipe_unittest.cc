// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include "base/memory/ref_counted.h"
#include "mojo/edk/system/test_utils.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace edk {
namespace {

const MojoHandleSignals kAllSignals = MOJO_HANDLE_SIGNAL_READABLE |
                                      MOJO_HANDLE_SIGNAL_WRITABLE |
                                      MOJO_HANDLE_SIGNAL_PEER_CLOSED;
static const char kHelloWorld[] = "hello world";

class MessagePipeTest : public test::MojoTestBase {
 public:
  MessagePipeTest() {
    CHECK_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &pipe0_, &pipe1_));
  }

  ~MessagePipeTest() override {
    if (pipe0_ != MOJO_HANDLE_INVALID)
      CHECK_EQ(MOJO_RESULT_OK, MojoClose(pipe0_));
    if (pipe1_ != MOJO_HANDLE_INVALID)
      CHECK_EQ(MOJO_RESULT_OK, MojoClose(pipe1_));
  }

  MojoResult WriteMessage(MojoHandle message_pipe_handle,
                          const void* bytes,
                          uint32_t num_bytes) {
    return MojoWriteMessage(message_pipe_handle, bytes, num_bytes, nullptr, 0,
                            MOJO_WRITE_MESSAGE_FLAG_NONE);
  }

  MojoResult ReadMessage(MojoHandle message_pipe_handle,
                         void* bytes,
                         uint32_t* num_bytes,
                         bool may_discard = false) {
    return MojoReadMessage(message_pipe_handle, bytes, num_bytes, nullptr, 0,
                           may_discard ? MOJO_READ_MESSAGE_FLAG_MAY_DISCARD :
                                         MOJO_READ_MESSAGE_FLAG_NONE);
  }

  MojoHandle pipe0_, pipe1_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessagePipeTest);
};

using FuseMessagePipeTest = test::MojoTestBase;

TEST_F(MessagePipeTest, WriteData) {
  ASSERT_EQ(MOJO_RESULT_OK,
            WriteMessage(pipe0_, kHelloWorld, sizeof(kHelloWorld)));
}

// Tests:
//  - only default flags
//  - reading messages from a port
//    - when there are no/one/two messages available for that port
//    - with buffer size 0 (and null buffer) -- should get size
//    - with too-small buffer -- should get size
//    - also verify that buffers aren't modified when/where they shouldn't be
//  - writing messages to a port
//    - in the obvious scenarios (as above)
//    - to a port that's been closed
//  - writing a message to a port, closing the other (would be the source) port,
//    and reading it
TEST_F(MessagePipeTest, Basic) {
  int32_t buffer[2];
  const uint32_t kBufferSize = static_cast<uint32_t>(sizeof(buffer));
  uint32_t buffer_size;

  // Nothing to read yet on port 0.
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT, ReadMessage(pipe0_, buffer, &buffer_size));
  ASSERT_EQ(kBufferSize, buffer_size);
  ASSERT_EQ(123, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  // Ditto for port 1.
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT, ReadMessage(pipe1_, buffer, &buffer_size));

  // Write from port 1 (to port 0).
  buffer[0] = 789012345;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe1_, buffer, sizeof(buffer[0])));

  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe0_, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));

  // Read from port 0.
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe0_, buffer, &buffer_size));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);
  ASSERT_EQ(789012345, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  // Read again from port 0 -- it should be empty.
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT, ReadMessage(pipe0_, buffer, &buffer_size));

  // Write two messages from port 0 (to port 1).
  buffer[0] = 123456789;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe0_, buffer, sizeof(buffer[0])));
  buffer[0] = 234567890;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe0_, buffer, sizeof(buffer[0])));

  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe1_, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));

  // Read from port 1 with buffer size 0 (should get the size of next message).
  // Also test that giving a null buffer is okay when the buffer size is 0.
  buffer_size = 0;
  ASSERT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            ReadMessage(pipe1_, nullptr, &buffer_size));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);

  // Read from port 1 with buffer size 1 (too small; should get the size of next
  // message).
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = 1;
  ASSERT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            ReadMessage(pipe1_, buffer, &buffer_size));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);
  ASSERT_EQ(123, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  // Read from port 1.
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe1_, buffer, &buffer_size));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);
  ASSERT_EQ(123456789, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe1_, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));

  // Read again from port 1.
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe1_, buffer, &buffer_size));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);
  ASSERT_EQ(234567890, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  // Read again from port 1 -- it should be empty.
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT, ReadMessage(pipe1_, buffer, &buffer_size));

  // Write from port 0 (to port 1).
  buffer[0] = 345678901;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe0_, buffer, sizeof(buffer[0])));

  // Close port 0.
  MojoClose(pipe0_);
  pipe0_ = MOJO_HANDLE_INVALID;

  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe1_, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                                     MOJO_DEADLINE_INDEFINITE, &state));

  // Try to write from port 1 (to port 0).
  buffer[0] = 456789012;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WriteMessage(pipe1_, buffer, sizeof(buffer[0])));

  // Read from port 1; should still get message (even though port 0 was closed).
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe1_, buffer, &buffer_size));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);
  ASSERT_EQ(345678901, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  // Read again from port 1 -- it should be empty (and port 0 is closed).
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            ReadMessage(pipe1_, buffer, &buffer_size));
}

TEST_F(MessagePipeTest, CloseWithQueuedIncomingMessages) {
  int32_t buffer[1];
  const uint32_t kBufferSize = static_cast<uint32_t>(sizeof(buffer));
  uint32_t buffer_size;

  // Write some messages from port 1 (to port 0).
  for (int32_t i = 0; i < 5; i++) {
    buffer[0] = i;
    ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe1_, buffer, kBufferSize));
  }

  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe0_, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));

  // Port 0 shouldn't be empty.
  buffer_size = 0;
  ASSERT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            ReadMessage(pipe0_, nullptr, &buffer_size));
  ASSERT_EQ(kBufferSize, buffer_size);

  // Close port 0 first, which should have outstanding (incoming) messages.
  MojoClose(pipe0_);
  MojoClose(pipe1_);
  pipe0_ = pipe1_ = MOJO_HANDLE_INVALID;
}

TEST_F(MessagePipeTest, DiscardMode) {
  int32_t buffer[2];
  const uint32_t kBufferSize = static_cast<uint32_t>(sizeof(buffer));
  uint32_t buffer_size;

  // Write from port 1 (to port 0).
  buffer[0] = 789012345;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe1_, buffer, sizeof(buffer[0])));

  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe0_, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));

  // Read/discard from port 0 (no buffer); get size.
  buffer_size = 0;
  ASSERT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            ReadMessage(pipe0_, nullptr, &buffer_size, true));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);

  // Read again from port 0 -- it should be empty.
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT,
            ReadMessage(pipe0_, buffer, &buffer_size, true));

  // Write from port 1 (to port 0).
  buffer[0] = 890123456;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            WriteMessage(pipe1_, buffer, sizeof(buffer[0])));

  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe0_, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));

  // Read from port 0 (buffer big enough).
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe0_, buffer, &buffer_size, true));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);
  ASSERT_EQ(890123456, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  // Read again from port 0 -- it should be empty.
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT,
            ReadMessage(pipe0_, buffer, &buffer_size, true));

  // Write from port 1 (to port 0).
  buffer[0] = 901234567;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe1_, buffer, sizeof(buffer[0])));

  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe0_, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));

  // Read/discard from port 0 (buffer too small); get size.
  buffer_size = 1;
  ASSERT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            ReadMessage(pipe0_, buffer, &buffer_size, true));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);

  // Read again from port 0 -- it should be empty.
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT,
            ReadMessage(pipe0_, buffer, &buffer_size, true));

  // Write from port 1 (to port 0).
  buffer[0] = 123456789;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe1_, buffer, sizeof(buffer[0])));

  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe0_, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));

  // Discard from port 0.
  buffer_size = 1;
  ASSERT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            ReadMessage(pipe0_, nullptr, 0, true));

  // Read again from port 0 -- it should be empty.
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT,
            ReadMessage(pipe0_, buffer, &buffer_size, true));
}

TEST_F(MessagePipeTest, BasicWaiting) {
  MojoHandleSignalsState hss;

  int32_t buffer[1];
  const uint32_t kBufferSize = static_cast<uint32_t>(sizeof(buffer));
  uint32_t buffer_size;

  // Always writable (until the other port is closed).
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe0_, MOJO_HANDLE_SIGNAL_WRITABLE, 0,
                                     &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  hss = MojoHandleSignalsState();

  // Not yet readable.
  ASSERT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            MojoWait(pipe0_, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);

  // The peer is not closed.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            MojoWait(pipe0_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, 0, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);

  // Write from port 0 (to port 1), to make port 1 readable.
  buffer[0] = 123456789;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe0_, buffer, kBufferSize));

  // Port 1 should already be readable now.
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe1_, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  // ... and still writable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe1_, MOJO_HANDLE_SIGNAL_WRITABLE,
                                     MOJO_DEADLINE_INDEFINITE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);

  // Close port 0.
  MojoClose(pipe0_);
  pipe0_ = MOJO_HANDLE_INVALID;

  // Port 1 should be signaled with peer closed.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe1_, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                                     MOJO_DEADLINE_INDEFINITE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Port 1 should not be writable.
  hss = MojoHandleSignalsState();

  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoWait(pipe1_, MOJO_HANDLE_SIGNAL_WRITABLE,
                     MOJO_DEADLINE_INDEFINITE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // But it should still be readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(pipe1_, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Read from port 1.
  buffer[0] = 0;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe1_, buffer, &buffer_size));
  ASSERT_EQ(123456789, buffer[0]);

  // Now port 1 should no longer be readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoWait(pipe1_, MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_DEADLINE_INDEFINITE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);
}

TEST_F(MessagePipeTest, InvalidMessageObjects) {
  // null message
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoFreeMessage(MOJO_MESSAGE_HANDLE_INVALID));

  // null message
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoGetMessageBuffer(MOJO_MESSAGE_HANDLE_INVALID, nullptr));

  // Non-zero num_handles with null handles array.
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAllocMessage(0, nullptr, 1, MOJO_ALLOC_MESSAGE_FLAG_NONE,
                             nullptr));
}

TEST_F(MessagePipeTest, AllocAndFreeMessage) {
  const std::string kMessage = "Hello, world.";
  MojoMessageHandle message = MOJO_MESSAGE_HANDLE_INVALID;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoAllocMessage(static_cast<uint32_t>(kMessage.size()), nullptr, 0,
                             MOJO_ALLOC_MESSAGE_FLAG_NONE, &message));
  ASSERT_NE(MOJO_MESSAGE_HANDLE_INVALID, message);
  ASSERT_EQ(MOJO_RESULT_OK, MojoFreeMessage(message));
}

TEST_F(MessagePipeTest, WriteAndReadMessageObject) {
  const std::string kMessage = "Hello, world.";
  MojoMessageHandle message = MOJO_MESSAGE_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAllocMessage(static_cast<uint32_t>(kMessage.size()), nullptr, 0,
                             MOJO_ALLOC_MESSAGE_FLAG_NONE, &message));
  ASSERT_NE(MOJO_MESSAGE_HANDLE_INVALID, message);

  void* buffer = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK, MojoGetMessageBuffer(message, &buffer));
  ASSERT_TRUE(buffer);
  memcpy(buffer, kMessage.data(), kMessage.size());

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessageNew(a, message, MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWait(b, MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE,
                     nullptr));
  uint32_t num_bytes = 0;
  uint32_t num_handles = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessageNew(b, &message, &num_bytes, nullptr, &num_handles,
                               MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_NE(MOJO_MESSAGE_HANDLE_INVALID, message);
  EXPECT_EQ(static_cast<uint32_t>(kMessage.size()), num_bytes);
  EXPECT_EQ(0u, num_handles);

  EXPECT_EQ(MOJO_RESULT_OK, MojoGetMessageBuffer(message, &buffer));
  ASSERT_TRUE(buffer);

  EXPECT_EQ(0, strncmp(static_cast<const char*>(buffer), kMessage.data(),
                       num_bytes));

  EXPECT_EQ(MOJO_RESULT_OK, MojoFreeMessage(message));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
}

#if !defined(OS_IOS)

const size_t kPingPongHandlesPerIteration = 50;
const size_t kPingPongIterations = 500;

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(HandlePingPong, MessagePipeTest, h) {
  // Waits for a handle to become readable and writes it back to the sender.
  for (size_t i = 0; i < kPingPongIterations; i++) {
    MojoHandle handles[kPingPongHandlesPerIteration];
    ReadMessageWithHandles(h, handles, kPingPongHandlesPerIteration);
    WriteMessageWithHandles(h, "", handles, kPingPongHandlesPerIteration);
  }

  EXPECT_EQ(MOJO_RESULT_OK, MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, nullptr));
  char msg[4];
  uint32_t num_bytes = 4;
  EXPECT_EQ(MOJO_RESULT_OK, ReadMessage(h, msg, &num_bytes));
}

// This test is flaky: http://crbug.com/585784
TEST_F(MessagePipeTest, DISABLED_DataPipeConsumerHandlePingPong) {
  MojoHandle p, c[kPingPongHandlesPerIteration];
  for (size_t i = 0; i < kPingPongHandlesPerIteration; ++i) {
    EXPECT_EQ(MOJO_RESULT_OK, MojoCreateDataPipe(nullptr, &p, &c[i]));
    MojoClose(p);
  }

  RUN_CHILD_ON_PIPE(HandlePingPong, h)
    for (size_t i = 0; i < kPingPongIterations; i++) {
      WriteMessageWithHandles(h, "", c, kPingPongHandlesPerIteration);
      ReadMessageWithHandles(h, c, kPingPongHandlesPerIteration);
    }
    WriteMessage(h, "quit", 4);
  END_CHILD()
  for (size_t i = 0; i < kPingPongHandlesPerIteration; ++i)
    MojoClose(c[i]);
}

// This test is flaky: http://crbug.com/585784
TEST_F(MessagePipeTest, DISABLED_DataPipeProducerHandlePingPong) {
  MojoHandle p[kPingPongHandlesPerIteration], c;
  for (size_t i = 0; i < kPingPongHandlesPerIteration; ++i) {
    EXPECT_EQ(MOJO_RESULT_OK, MojoCreateDataPipe(nullptr, &p[i], &c));
    MojoClose(c);
  }

  RUN_CHILD_ON_PIPE(HandlePingPong, h)
    for (size_t i = 0; i < kPingPongIterations; i++) {
      WriteMessageWithHandles(h, "", p, kPingPongHandlesPerIteration);
      ReadMessageWithHandles(h, p, kPingPongHandlesPerIteration);
    }
    WriteMessage(h, "quit", 4);
  END_CHILD()
  for (size_t i = 0; i < kPingPongHandlesPerIteration; ++i)
    MojoClose(p[i]);
}

TEST_F(MessagePipeTest, SharedBufferHandlePingPong) {
  MojoHandle buffers[kPingPongHandlesPerIteration];
  for (size_t i = 0; i <kPingPongHandlesPerIteration; ++i)
    EXPECT_EQ(MOJO_RESULT_OK, MojoCreateSharedBuffer(nullptr, 1, &buffers[i]));

  RUN_CHILD_ON_PIPE(HandlePingPong, h)
    for (size_t i = 0; i < kPingPongIterations; i++) {
      WriteMessageWithHandles(h, "", buffers, kPingPongHandlesPerIteration);
      ReadMessageWithHandles(h, buffers, kPingPongHandlesPerIteration);
    }
    WriteMessage(h, "quit", 4);
  END_CHILD()
  for (size_t i = 0; i < kPingPongHandlesPerIteration; ++i)
    MojoClose(buffers[i]);
}

#endif  // !defined(OS_IOS)

TEST_F(FuseMessagePipeTest, Basic) {
  // Test that we can fuse pipes and they still work.

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  EXPECT_EQ(MOJO_RESULT_OK, MojoFuseMessagePipes(b, c));

  // Handles b and c should be closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(c));

  const std::string kTestMessage1 = "Hello, world!";
  const std::string kTestMessage2 = "Goodbye, world!";

  WriteMessage(a, kTestMessage1);
  EXPECT_EQ(kTestMessage1, ReadMessage(d));

  WriteMessage(d, kTestMessage2);
  EXPECT_EQ(kTestMessage2, ReadMessage(a));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(FuseMessagePipeTest, FuseAfterPeerWrite) {
  // Test that messages written before fusion are eventually delivered.

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  const std::string kTestMessage1 = "Hello, world!";
  const std::string kTestMessage2 = "Goodbye, world!";
  WriteMessage(a, kTestMessage1);
  WriteMessage(d, kTestMessage2);

  EXPECT_EQ(MOJO_RESULT_OK, MojoFuseMessagePipes(b, c));

  // Handles b and c should be closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(c));

  EXPECT_EQ(kTestMessage1, ReadMessage(d));
  EXPECT_EQ(kTestMessage2, ReadMessage(a));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(FuseMessagePipeTest, NoFuseAfterWrite) {
  // Test that a pipe endpoint which has been written to cannot be fused.

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  WriteMessage(b, "shouldn't have done that!");
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, MojoFuseMessagePipes(b, c));

  // Handles b and c should be closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(c));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(FuseMessagePipeTest, NoFuseSelf) {
  // Test that a pipe's own endpoints can't be fused together.

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, MojoFuseMessagePipes(a, b));

  // Handles a and b should be closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(b));
}

TEST_F(FuseMessagePipeTest, FuseInvalidArguments) {
  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));

  // Can't fuse an invalid handle.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoFuseMessagePipes(b, c));

  // Handle c should be closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(c));

  // Can't fuse a non-message pipe handle.
  MojoHandle e, f;
  CreateDataPipe(&e, &f, 16);

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoFuseMessagePipes(e, d));

  // Handles d and e should be closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(d));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(e));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(f));
}

TEST_F(FuseMessagePipeTest, FuseAfterPeerClosure) {
  // Test that peer closure prior to fusion can still be detected after fusion.

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoFuseMessagePipes(b, c));

  // Handles b and c should be closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(c));

  EXPECT_EQ(MOJO_RESULT_OK, MojoWait(d, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                                     MOJO_DEADLINE_INDEFINITE, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(FuseMessagePipeTest, FuseAfterPeerWriteAndClosure) {
  // Test that peer write and closure prior to fusion still results in the
  // both message arrival and awareness of peer closure.

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  const std::string kTestMessage = "ayyy lmao";
  WriteMessage(a, kTestMessage);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));

  EXPECT_EQ(MOJO_RESULT_OK, MojoFuseMessagePipes(b, c));

  // Handles b and c should be closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(c));

  EXPECT_EQ(kTestMessage, ReadMessage(d));
  EXPECT_EQ(MOJO_RESULT_OK, MojoWait(d, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                                     MOJO_DEADLINE_INDEFINITE, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

}  // namespace
}  // namespace edk
}  // namespace mojo
