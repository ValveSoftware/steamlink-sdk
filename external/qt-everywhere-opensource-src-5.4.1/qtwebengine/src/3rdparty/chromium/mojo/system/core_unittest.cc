// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/core.h"

#include <limits>

#include "base/basictypes.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "mojo/system/core_test_base.h"

namespace mojo {
namespace system {
namespace {

typedef test::CoreTestBase CoreTest;

TEST_F(CoreTest, GetTimeTicksNow) {
  const MojoTimeTicks start = core()->GetTimeTicksNow();
  EXPECT_NE(static_cast<MojoTimeTicks>(0), start)
      << "GetTimeTicksNow should return nonzero value";
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(15));
  const MojoTimeTicks finish = core()->GetTimeTicksNow();
  // Allow for some fuzz in sleep.
  EXPECT_GE((finish - start), static_cast<MojoTimeTicks>(8000))
      << "Sleeping should result in increasing time ticks";
}

TEST_F(CoreTest, Basic) {
  MockHandleInfo info;

  EXPECT_EQ(0u, info.GetCtorCallCount());
  MojoHandle h = CreateMockHandle(&info);
  EXPECT_EQ(1u, info.GetCtorCallCount());
  EXPECT_NE(h, MOJO_HANDLE_INVALID);

  EXPECT_EQ(0u, info.GetWriteMessageCallCount());
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h, NULL, 0, NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(1u, info.GetWriteMessageCallCount());
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            core()->WriteMessage(h, NULL, 1, NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(2u, info.GetWriteMessageCallCount());

  EXPECT_EQ(0u, info.GetReadMessageCallCount());
  uint32_t num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h, NULL, &num_bytes, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(1u, info.GetReadMessageCallCount());
  num_bytes = 1;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            core()->ReadMessage(h, NULL, &num_bytes, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(2u, info.GetReadMessageCallCount());
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h, NULL, NULL, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(3u, info.GetReadMessageCallCount());

  EXPECT_EQ(0u, info.GetWriteDataCallCount());
  EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->WriteData(h, NULL, NULL, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(1u, info.GetWriteDataCallCount());

  EXPECT_EQ(0u, info.GetBeginWriteDataCallCount());
  EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->BeginWriteData(h, NULL, NULL, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(1u, info.GetBeginWriteDataCallCount());

  EXPECT_EQ(0u, info.GetEndWriteDataCallCount());
  EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->EndWriteData(h, 0));
  EXPECT_EQ(1u, info.GetEndWriteDataCallCount());

  EXPECT_EQ(0u, info.GetReadDataCallCount());
  EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->ReadData(h, NULL, NULL, MOJO_READ_DATA_FLAG_NONE));
  EXPECT_EQ(1u, info.GetReadDataCallCount());

  EXPECT_EQ(0u, info.GetBeginReadDataCallCount());
  EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->BeginReadData(h, NULL, NULL, MOJO_READ_DATA_FLAG_NONE));
  EXPECT_EQ(1u, info.GetBeginReadDataCallCount());

  EXPECT_EQ(0u, info.GetEndReadDataCallCount());
  EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->EndReadData(h, 0));
  EXPECT_EQ(1u, info.GetEndReadDataCallCount());

  EXPECT_EQ(0u, info.GetAddWaiterCallCount());
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h, ~MOJO_HANDLE_SIGNAL_NONE,
                         MOJO_DEADLINE_INDEFINITE));
  EXPECT_EQ(1u, info.GetAddWaiterCallCount());
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h, ~MOJO_HANDLE_SIGNAL_NONE, 0));
  EXPECT_EQ(2u, info.GetAddWaiterCallCount());
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h, ~MOJO_HANDLE_SIGNAL_NONE, 10 * 1000));
  EXPECT_EQ(3u, info.GetAddWaiterCallCount());
  MojoHandleSignals handle_signals = ~MOJO_HANDLE_SIGNAL_NONE;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->WaitMany(&h, &handle_signals, 1, MOJO_DEADLINE_INDEFINITE));
  EXPECT_EQ(4u, info.GetAddWaiterCallCount());

  EXPECT_EQ(0u, info.GetDtorCallCount());
  EXPECT_EQ(0u, info.GetCloseCallCount());
  EXPECT_EQ(0u, info.GetCancelAllWaitersCallCount());
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h));
  EXPECT_EQ(1u, info.GetCancelAllWaitersCallCount());
  EXPECT_EQ(1u, info.GetCloseCallCount());
  EXPECT_EQ(1u, info.GetDtorCallCount());

  // No waiters should ever have ever been added.
  EXPECT_EQ(0u, info.GetRemoveWaiterCallCount());
}

TEST_F(CoreTest, InvalidArguments) {
  // |Close()|:
  {
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(MOJO_HANDLE_INVALID));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(10));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(1000000000));

    // Test a double-close.
    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);
    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h));
    EXPECT_EQ(1u, info.GetCloseCallCount());
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(h));
    EXPECT_EQ(1u, info.GetCloseCallCount());
  }

  // |Wait()|:
  {
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->Wait(MOJO_HANDLE_INVALID, ~MOJO_HANDLE_SIGNAL_NONE,
                           MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->Wait(10, ~MOJO_HANDLE_SIGNAL_NONE,
                           MOJO_DEADLINE_INDEFINITE));
  }

  // |WaitMany()|:
  {
    MojoHandle handles[2] = {MOJO_HANDLE_INVALID, MOJO_HANDLE_INVALID};
    MojoHandleSignals signals[2] = {~MOJO_HANDLE_SIGNAL_NONE,
                                    ~MOJO_HANDLE_SIGNAL_NONE};
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, signals, 0, MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(NULL, signals, 0, MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, NULL, 0, MOJO_DEADLINE_INDEFINITE));

    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(NULL, signals, 1, MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, NULL, 1, MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, signals, 1, MOJO_DEADLINE_INDEFINITE));

    MockHandleInfo info[2];
    handles[0] = CreateMockHandle(&info[0]);

    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              core()->WaitMany(handles, signals, 1, MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, signals, 2, MOJO_DEADLINE_INDEFINITE));
    handles[1] = handles[0] + 1;  // Invalid handle.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, signals, 2, MOJO_DEADLINE_INDEFINITE));
    handles[1] = CreateMockHandle(&info[1]);
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              core()->WaitMany(handles, signals, 2, MOJO_DEADLINE_INDEFINITE));

    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(handles[0]));
    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(handles[1]));
  }

  // |CreateMessagePipe()|:
  {
    MojoHandle h;
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->CreateMessagePipe(NULL, NULL, NULL));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->CreateMessagePipe(NULL, &h, NULL));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->CreateMessagePipe(NULL, NULL, &h));
  }

  // |WriteMessage()|:
  // Only check arguments checked by |Core|, namely |handle|, |handles|, and
  // |num_handles|.
  {
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(MOJO_HANDLE_INVALID, NULL, 0, NULL, 0,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));

    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);
    MojoHandle handles[2] = {MOJO_HANDLE_INVALID, MOJO_HANDLE_INVALID};

    // Null |handles| with nonzero |num_handles|.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(h, NULL, 0, NULL, 1,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    // Checked by |Core|, shouldn't go through to the dispatcher.
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    // Huge handle count (implausibly big on some systems -- more than can be
    // stored in a 32-bit address space).
    // Note: This may return either |MOJO_RESULT_INVALID_ARGUMENT| or
    // |MOJO_RESULT_RESOURCE_EXHAUSTED|, depending on whether it's plausible or
    // not.
    EXPECT_NE(MOJO_RESULT_OK,
              core()->WriteMessage(h, NULL, 0, handles,
                                   std::numeric_limits<uint32_t>::max(),
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    // Huge handle count (plausibly big).
    EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
              core()->WriteMessage(h, NULL, 0, handles,
                                   std::numeric_limits<uint32_t>::max() /
                                       sizeof(handles[0]),
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    // Invalid handle in |handles|.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(h, NULL, 0, handles, 1,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    // Two invalid handles in |handles|.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(h, NULL, 0, handles, 2,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    // Can't send a handle over itself.
    handles[0] = h;
    EXPECT_EQ(MOJO_RESULT_BUSY,
              core()->WriteMessage(h, NULL, 0, handles, 1,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    MockHandleInfo info2;
    MojoHandle h2 = CreateMockHandle(&info2);

    // This is "okay", but |MockDispatcher| doesn't implement it.
    handles[0] = h2;
    EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
              core()->WriteMessage(h, NULL, 0, handles, 1,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(1u, info.GetWriteMessageCallCount());

    // One of the |handles| is still invalid.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(h, NULL, 0, handles, 2,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(1u, info.GetWriteMessageCallCount());

    // One of the |handles| is the same as |handle|.
    handles[1] = h;
    EXPECT_EQ(MOJO_RESULT_BUSY,
              core()->WriteMessage(h, NULL, 0, handles, 2,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(1u, info.GetWriteMessageCallCount());

    // Can't send a handle twice in the same message.
    handles[1] = h2;
    EXPECT_EQ(MOJO_RESULT_BUSY,
              core()->WriteMessage(h, NULL, 0, handles, 2,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(1u, info.GetWriteMessageCallCount());

    // Note: Since we never successfully sent anything with it, |h2| should
    // still be valid.
    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h2));

    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h));
  }

  // |ReadMessage()|:
  // Only check arguments checked by |Core|, namely |handle|, |handles|, and
  // |num_handles|.
  {
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->ReadMessage(MOJO_HANDLE_INVALID, NULL, NULL, NULL, NULL,
                                  MOJO_READ_MESSAGE_FLAG_NONE));

    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);

    uint32_t handle_count = 1;
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->ReadMessage(h, NULL, NULL, NULL, &handle_count,
                                  MOJO_READ_MESSAGE_FLAG_NONE));
    // Checked by |Core|, shouldn't go through to the dispatcher.
    EXPECT_EQ(0u, info.GetReadMessageCallCount());

    // Okay.
    handle_count = 0;
    EXPECT_EQ(MOJO_RESULT_OK,
              core()->ReadMessage(h, NULL, NULL, NULL, &handle_count,
                                  MOJO_READ_MESSAGE_FLAG_NONE));
    // Checked by |Core|, shouldn't go through to the dispatcher.
    EXPECT_EQ(1u, info.GetReadMessageCallCount());

    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h));
  }
}

// TODO(vtl): test |Wait()| and |WaitMany()| properly
//  - including |WaitMany()| with the same handle more than once (with
//    same/different signals)

TEST_F(CoreTest, MessagePipe) {
  MojoHandle h[2];

  EXPECT_EQ(MOJO_RESULT_OK, core()->CreateMessagePipe(NULL, &h[0], &h[1]));
  // Should get two distinct, valid handles.
  EXPECT_NE(h[0], MOJO_HANDLE_INVALID);
  EXPECT_NE(h[1], MOJO_HANDLE_INVALID);
  EXPECT_NE(h[0], h[1]);

  // Neither should be readable.
  MojoHandleSignals signals[2] = {MOJO_HANDLE_SIGNAL_READABLE,
                                  MOJO_HANDLE_SIGNAL_READABLE};
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            core()->WaitMany(h, signals, 2, 0));

  // Try to read anyway.
  char buffer[1] = {'a'};
  uint32_t buffer_size = 1;
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            core()->ReadMessage(h[0], buffer, &buffer_size, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  // Check that it left its inputs alone.
  EXPECT_EQ('a', buffer[0]);
  EXPECT_EQ(1u, buffer_size);

  // Both should be writable.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h[0], MOJO_HANDLE_SIGNAL_WRITABLE, 1000000000));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h[1], MOJO_HANDLE_SIGNAL_WRITABLE, 1000000000));

  // Also check that |h[1]| is writable using |WaitMany()|.
  signals[0] = MOJO_HANDLE_SIGNAL_READABLE;
  signals[1] = MOJO_HANDLE_SIGNAL_WRITABLE;
  EXPECT_EQ(1, core()->WaitMany(h, signals, 2, MOJO_DEADLINE_INDEFINITE));

  // Write to |h[1]|.
  buffer[0] = 'b';
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h[1], buffer, 1, NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Check that |h[0]| is now readable.
  signals[0] = MOJO_HANDLE_SIGNAL_READABLE;
  signals[1] = MOJO_HANDLE_SIGNAL_READABLE;
  EXPECT_EQ(0, core()->WaitMany(h, signals, 2, MOJO_DEADLINE_INDEFINITE));

  // Read from |h[0]|.
  // First, get only the size.
  buffer_size = 0;
  EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            core()->ReadMessage(h[0], NULL, &buffer_size, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(1u, buffer_size);
  // Then actually read it.
  buffer[0] = 'c';
  buffer_size = 1;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h[0], buffer, &buffer_size, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ('b', buffer[0]);
  EXPECT_EQ(1u, buffer_size);

  // |h[0]| should no longer be readable.
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            core()->Wait(h[0], MOJO_HANDLE_SIGNAL_READABLE, 0));

  // Write to |h[0]|.
  buffer[0] = 'd';
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h[0], buffer, 1, NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Close |h[0]|.
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h[0]));

  // Check that |h[1]| is no longer writable (and will never be).
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h[1], MOJO_HANDLE_SIGNAL_WRITABLE, 1000000000));

  // Check that |h[1]| is still readable (for the moment).
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h[1], MOJO_HANDLE_SIGNAL_READABLE, 1000000000));

  // Discard a message from |h[1]|.
  EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            core()->ReadMessage(h[1], NULL, NULL, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_MAY_DISCARD));

  // |h[1]| is no longer readable (and will never be).
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h[1], MOJO_HANDLE_SIGNAL_READABLE, 1000000000));

  // Try writing to |h[1]|.
  buffer[0] = 'e';
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->WriteMessage(h[1], buffer, 1, NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h[1]));
}

// Tests passing a message pipe handle.
TEST_F(CoreTest, MessagePipeBasicLocalHandlePassing1) {
  const char kHello[] = "hello";
  const uint32_t kHelloSize = static_cast<uint32_t>(sizeof(kHello));
  const char kWorld[] = "world!!!";
  const uint32_t kWorldSize = static_cast<uint32_t>(sizeof(kWorld));
  char buffer[100];
  const uint32_t kBufferSize = static_cast<uint32_t>(sizeof(buffer));
  uint32_t num_bytes;
  MojoHandle handles[10];
  uint32_t num_handles;
  MojoHandle h_received;

  MojoHandle h_passing[2];
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->CreateMessagePipe(NULL, &h_passing[0], &h_passing[1]));

  // Make sure that |h_passing[]| work properly.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0],
                                 kHello, kHelloSize,
                                 NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE,
                         1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_passing[1],
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kHelloSize, num_bytes);
  EXPECT_STREQ(kHello, buffer);
  EXPECT_EQ(0u, num_handles);

  // Make sure that you can't pass either of the message pipe's handles over
  // itself.
  EXPECT_EQ(MOJO_RESULT_BUSY,
            core()->WriteMessage(h_passing[0],
                                 kHello, kHelloSize,
                                 &h_passing[0], 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            core()->WriteMessage(h_passing[0],
                                 kHello, kHelloSize,
                                 &h_passing[1], 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));

  MojoHandle h_passed[2];
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->CreateMessagePipe(NULL, &h_passed[0], &h_passed[1]));

  // Make sure that |h_passed[]| work properly.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passed[0],
                                 kHello, kHelloSize,
                                 NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passed[1], MOJO_HANDLE_SIGNAL_READABLE, 1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_passed[1],
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kHelloSize, num_bytes);
  EXPECT_STREQ(kHello, buffer);
  EXPECT_EQ(0u, num_handles);

  // Send |h_passed[1]| from |h_passing[0]| to |h_passing[1]|.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0],
                                 kWorld, kWorldSize,
                                 &h_passed[1], 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE,
                         1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_passing[1],
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kWorldSize, num_bytes);
  EXPECT_STREQ(kWorld, buffer);
  EXPECT_EQ(1u, num_handles);
  h_received = handles[0];
  EXPECT_NE(h_received, MOJO_HANDLE_INVALID);
  EXPECT_NE(h_received, h_passing[0]);
  EXPECT_NE(h_received, h_passing[1]);
  EXPECT_NE(h_received, h_passed[0]);

  // Note: We rely on the Mojo system not re-using handle values very often.
  EXPECT_NE(h_received, h_passed[1]);

  // |h_passed[1]| should no longer be valid; check that trying to close it
  // fails. See above note.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(h_passed[1]));

  // Write to |h_passed[0]|. Should receive on |h_received|.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passed[0],
                                 kHello, kHelloSize,
                                 NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_received, MOJO_HANDLE_SIGNAL_READABLE, 1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_received,
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kHelloSize, num_bytes);
  EXPECT_STREQ(kHello, buffer);
  EXPECT_EQ(0u, num_handles);

  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[0]));
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[1]));
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h_passed[0]));
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h_received));
}

TEST_F(CoreTest, DataPipe) {
  MojoHandle ph, ch;  // p is for producer and c is for consumer.

  EXPECT_EQ(MOJO_RESULT_OK, core()->CreateDataPipe(NULL, &ph, &ch));
  // Should get two distinct, valid handles.
  EXPECT_NE(ph, MOJO_HANDLE_INVALID);
  EXPECT_NE(ch, MOJO_HANDLE_INVALID);
  EXPECT_NE(ph, ch);

  // Producer should be never-readable, but already writable.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(ph, MOJO_HANDLE_SIGNAL_READABLE, 0));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(ph, MOJO_HANDLE_SIGNAL_WRITABLE, 0));

  // Consumer should be never-writable, and not yet readable.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(ch, MOJO_HANDLE_SIGNAL_WRITABLE, 0));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 0));

  // Write.
  char elements[2] = {'A', 'B'};
  uint32_t num_bytes = 2u;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteData(ph, elements, &num_bytes,
                              MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(2u, num_bytes);

  // Consumer should now be readable.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 0));

  // Read one character.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = 1u;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch, elements, &num_bytes,
                             MOJO_READ_DATA_FLAG_NONE));
  EXPECT_EQ('A', elements[0]);
  EXPECT_EQ(-1, elements[1]);

  // Two-phase write.
  void* write_ptr = NULL;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->BeginWriteData(ph, &write_ptr, &num_bytes,
                                   MOJO_WRITE_DATA_FLAG_NONE));
  // We count on the default options providing a decent buffer size.
  ASSERT_GE(num_bytes, 3u);

  // Trying to do a normal write during a two-phase write should fail.
  elements[0] = 'X';
  num_bytes = 1u;
  EXPECT_EQ(MOJO_RESULT_BUSY,
            core()->WriteData(ph, elements, &num_bytes,
                              MOJO_WRITE_DATA_FLAG_NONE));

  // Actually write the data, and complete it now.
  static_cast<char*>(write_ptr)[0] = 'C';
  static_cast<char*>(write_ptr)[1] = 'D';
  static_cast<char*>(write_ptr)[2] = 'E';
  EXPECT_EQ(MOJO_RESULT_OK, core()->EndWriteData(ph, 3u));

  // Query how much data we have.
  num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch, NULL, &num_bytes, MOJO_READ_DATA_FLAG_QUERY));
  EXPECT_EQ(4u, num_bytes);

  // Try to discard ten characters, in all-or-none mode. Should fail.
  num_bytes = 10;
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            core()->ReadData(ch, NULL, &num_bytes,
                             MOJO_READ_DATA_FLAG_DISCARD |
                                 MOJO_READ_DATA_FLAG_ALL_OR_NONE));

  // Discard two characters.
  num_bytes = 2;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch, NULL, &num_bytes,
                             MOJO_READ_DATA_FLAG_DISCARD |
                                 MOJO_READ_DATA_FLAG_ALL_OR_NONE));

  // Read the remaining two characters, in two-phase mode (all-or-none).
  const void* read_ptr = NULL;
  num_bytes = 2;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->BeginReadData(ch, &read_ptr, &num_bytes,
                                  MOJO_READ_DATA_FLAG_ALL_OR_NONE));
  // Note: Count on still being able to do the contiguous read here.
  ASSERT_EQ(2u, num_bytes);

  // Discarding right now should fail.
  num_bytes = 1;
  EXPECT_EQ(MOJO_RESULT_BUSY,
            core()->ReadData(ch, NULL, &num_bytes,
                             MOJO_READ_DATA_FLAG_DISCARD));

  // Actually check our data and end the two-phase read.
  EXPECT_EQ('D', static_cast<const char*>(read_ptr)[0]);
  EXPECT_EQ('E', static_cast<const char*>(read_ptr)[1]);
  EXPECT_EQ(MOJO_RESULT_OK, core()->EndReadData(ch, 2u));

  // Consumer should now be no longer readable.
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 0));

  // TODO(vtl): More.

  // Close the producer.
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(ph));

  // The consumer should now be never-readable.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 0));

  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(ch));
}

// Tests passing data pipe producer and consumer handles.
TEST_F(CoreTest, MessagePipeBasicLocalHandlePassing2) {
  const char kHello[] = "hello";
  const uint32_t kHelloSize = static_cast<uint32_t>(sizeof(kHello));
  const char kWorld[] = "world!!!";
  const uint32_t kWorldSize = static_cast<uint32_t>(sizeof(kWorld));
  char buffer[100];
  const uint32_t kBufferSize = static_cast<uint32_t>(sizeof(buffer));
  uint32_t num_bytes;
  MojoHandle handles[10];
  uint32_t num_handles;

  MojoHandle h_passing[2];
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->CreateMessagePipe(NULL, &h_passing[0], &h_passing[1]));

  MojoHandle ph, ch;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->CreateDataPipe(NULL, &ph, &ch));

  // Send |ch| from |h_passing[0]| to |h_passing[1]|.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0],
                                 kHello, kHelloSize,
                                 &ch, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE,
                         1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_passing[1],
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kHelloSize, num_bytes);
  EXPECT_STREQ(kHello, buffer);
  EXPECT_EQ(1u, num_handles);
  MojoHandle ch_received = handles[0];
  EXPECT_NE(ch_received, MOJO_HANDLE_INVALID);
  EXPECT_NE(ch_received, h_passing[0]);
  EXPECT_NE(ch_received, h_passing[1]);
  EXPECT_NE(ch_received, ph);

  // Note: We rely on the Mojo system not re-using handle values very often.
  EXPECT_NE(ch_received, ch);

  // |ch| should no longer be valid; check that trying to close it fails. See
  // above note.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(ch));

  // Write to |ph|. Should receive on |ch_received|.
  num_bytes = kWorldSize;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteData(ph, kWorld, &num_bytes,
                              MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(ch_received, MOJO_HANDLE_SIGNAL_READABLE, 1000000000));
  num_bytes = kBufferSize;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch_received, buffer, &num_bytes,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kWorldSize, num_bytes);
  EXPECT_STREQ(kWorld, buffer);

  // Now pass |ph| in the same direction.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0],
                                 kWorld, kWorldSize,
                                 &ph, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE,
                         1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_passing[1],
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kWorldSize, num_bytes);
  EXPECT_STREQ(kWorld, buffer);
  EXPECT_EQ(1u, num_handles);
  MojoHandle ph_received = handles[0];
  EXPECT_NE(ph_received, MOJO_HANDLE_INVALID);
  EXPECT_NE(ph_received, h_passing[0]);
  EXPECT_NE(ph_received, h_passing[1]);
  EXPECT_NE(ph_received, ch_received);

  // Again, rely on the Mojo system not re-using handle values very often.
  EXPECT_NE(ph_received, ph);

  // |ph| should no longer be valid; check that trying to close it fails. See
  // above note.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(ph));

  // Write to |ph_received|. Should receive on |ch_received|.
  num_bytes = kHelloSize;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteData(ph_received, kHello, &num_bytes,
                              MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(ch_received, MOJO_HANDLE_SIGNAL_READABLE, 1000000000));
  num_bytes = kBufferSize;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch_received, buffer, &num_bytes,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kHelloSize, num_bytes);
  EXPECT_STREQ(kHello, buffer);

  ph = ph_received;
  ph_received = MOJO_HANDLE_INVALID;
  ch = ch_received;
  ch_received = MOJO_HANDLE_INVALID;

  // Make sure that |ph| can't be sent if it's in a two-phase write.
  void* write_ptr = NULL;
  num_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->BeginWriteData(ph, &write_ptr, &num_bytes,
                                   MOJO_WRITE_DATA_FLAG_NONE));
  ASSERT_GE(num_bytes, 1u);
  EXPECT_EQ(MOJO_RESULT_BUSY,
            core()->WriteMessage(h_passing[0],
                                 kHello, kHelloSize,
                                 &ph, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));

  // But |ch| can, even if |ph| is in a two-phase write.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0],
                                 kHello, kHelloSize,
                                 &ch, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  ch = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE,
                         1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_passing[1],
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kHelloSize, num_bytes);
  EXPECT_STREQ(kHello, buffer);
  EXPECT_EQ(1u, num_handles);
  ch = handles[0];
  EXPECT_NE(ch, MOJO_HANDLE_INVALID);

  // Complete the two-phase write.
  static_cast<char*>(write_ptr)[0] = 'x';
  EXPECT_EQ(MOJO_RESULT_OK, core()->EndWriteData(ph, 1));

  // Wait for |ch| to be readable.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 1000000000));

  // Make sure that |ch| can't be sent if it's in a two-phase read.
  const void* read_ptr = NULL;
  num_bytes = 1;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->BeginReadData(ch, &read_ptr, &num_bytes,
                                  MOJO_READ_DATA_FLAG_ALL_OR_NONE));
  EXPECT_EQ(MOJO_RESULT_BUSY,
            core()->WriteMessage(h_passing[0],
                                 kHello, kHelloSize,
                                 &ch, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));

  // But |ph| can, even if |ch| is in a two-phase read.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0],
                                 kWorld, kWorldSize,
                                 &ph, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  ph = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE,
                         1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_passing[1],
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kWorldSize, num_bytes);
  EXPECT_STREQ(kWorld, buffer);
  EXPECT_EQ(1u, num_handles);
  ph = handles[0];
  EXPECT_NE(ph, MOJO_HANDLE_INVALID);

  // Complete the two-phase read.
  EXPECT_EQ('x', static_cast<const char*>(read_ptr)[0]);
  EXPECT_EQ(MOJO_RESULT_OK, core()->EndReadData(ch, 1));

  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[0]));
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[1]));
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(ph));
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(ch));
}

// TODO(vtl): Test |DuplicateBufferHandle()| and |MapBuffer()|.

}  // namespace
}  // namespace system
}  // namespace mojo
