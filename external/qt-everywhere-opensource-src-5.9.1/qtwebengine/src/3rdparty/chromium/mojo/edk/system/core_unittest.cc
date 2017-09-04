// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/core.h"

#include <stdint.h>

#include <limits>

#include "base/bind.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/awakable.h"
#include "mojo/edk/system/core_test_base.h"
#include "mojo/edk/system/test_utils.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace mojo {
namespace edk {
namespace {

const MojoHandleSignalsState kEmptyMojoHandleSignalsState = {0u, 0u};
const MojoHandleSignalsState kFullMojoHandleSignalsState = {~0u, ~0u};
const MojoHandleSignals kAllSignals = MOJO_HANDLE_SIGNAL_READABLE |
                                      MOJO_HANDLE_SIGNAL_WRITABLE |
                                      MOJO_HANDLE_SIGNAL_PEER_CLOSED;

using CoreTest = test::CoreTestBase;

TEST_F(CoreTest, GetTimeTicksNow) {
  const MojoTimeTicks start = core()->GetTimeTicksNow();
  ASSERT_NE(static_cast<MojoTimeTicks>(0), start)
      << "GetTimeTicksNow should return nonzero value";
  test::Sleep(test::DeadlineFromMilliseconds(15));
  const MojoTimeTicks finish = core()->GetTimeTicksNow();
  // Allow for some fuzz in sleep.
  ASSERT_GE((finish - start), static_cast<MojoTimeTicks>(8000))
      << "Sleeping should result in increasing time ticks";
}

TEST_F(CoreTest, Basic) {
  MockHandleInfo info;

  ASSERT_EQ(0u, info.GetCtorCallCount());
  MojoHandle h = CreateMockHandle(&info);
  ASSERT_EQ(1u, info.GetCtorCallCount());
  ASSERT_NE(h, MOJO_HANDLE_INVALID);

  ASSERT_EQ(0u, info.GetWriteMessageCallCount());
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h, nullptr, 0, nullptr, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  ASSERT_EQ(1u, info.GetWriteMessageCallCount());

  ASSERT_EQ(0u, info.GetReadMessageCallCount());
  uint32_t num_bytes = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      core()->ReadMessage(h, nullptr, &num_bytes, nullptr, nullptr,
                          MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(1u, info.GetReadMessageCallCount());
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h, nullptr, nullptr, nullptr, nullptr,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(2u, info.GetReadMessageCallCount());

  ASSERT_EQ(0u, info.GetWriteDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->WriteData(h, nullptr, nullptr, MOJO_WRITE_DATA_FLAG_NONE));
  ASSERT_EQ(1u, info.GetWriteDataCallCount());

  ASSERT_EQ(0u, info.GetBeginWriteDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->BeginWriteData(h, nullptr, nullptr,
                                   MOJO_WRITE_DATA_FLAG_NONE));
  ASSERT_EQ(1u, info.GetBeginWriteDataCallCount());

  ASSERT_EQ(0u, info.GetEndWriteDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED, core()->EndWriteData(h, 0));
  ASSERT_EQ(1u, info.GetEndWriteDataCallCount());

  ASSERT_EQ(0u, info.GetReadDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->ReadData(h, nullptr, nullptr, MOJO_READ_DATA_FLAG_NONE));
  ASSERT_EQ(1u, info.GetReadDataCallCount());

  ASSERT_EQ(0u, info.GetBeginReadDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->BeginReadData(h, nullptr, nullptr,
                                  MOJO_READ_DATA_FLAG_NONE));
  ASSERT_EQ(1u, info.GetBeginReadDataCallCount());

  ASSERT_EQ(0u, info.GetEndReadDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED, core()->EndReadData(h, 0));
  ASSERT_EQ(1u, info.GetEndReadDataCallCount());

  ASSERT_EQ(0u, info.GetAddAwakableCallCount());
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h, ~MOJO_HANDLE_SIGNAL_NONE, MOJO_DEADLINE_INDEFINITE,
                         nullptr));
  ASSERT_EQ(1u, info.GetAddAwakableCallCount());
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h, ~MOJO_HANDLE_SIGNAL_NONE, 0, nullptr));
  ASSERT_EQ(2u, info.GetAddAwakableCallCount());
  MojoHandleSignalsState hss = kFullMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h, ~MOJO_HANDLE_SIGNAL_NONE, MOJO_DEADLINE_INDEFINITE,
                         &hss));
  ASSERT_EQ(3u, info.GetAddAwakableCallCount());
  ASSERT_EQ(0u, hss.satisfied_signals);
  ASSERT_EQ(0u, hss.satisfiable_signals);
  ASSERT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      core()->Wait(h, ~MOJO_HANDLE_SIGNAL_NONE, 10 * 1000, nullptr));
  ASSERT_EQ(4u, info.GetAddAwakableCallCount());
  hss = kFullMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h, ~MOJO_HANDLE_SIGNAL_NONE, 10 * 1000, &hss));
  ASSERT_EQ(5u, info.GetAddAwakableCallCount());
  ASSERT_EQ(0u, hss.satisfied_signals);
  ASSERT_EQ(0u, hss.satisfiable_signals);

  MojoHandleSignals handle_signals = ~MOJO_HANDLE_SIGNAL_NONE;
  ASSERT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      core()->WaitMany(&h, &handle_signals, 1, MOJO_DEADLINE_INDEFINITE,
                       nullptr, nullptr));
  ASSERT_EQ(6u, info.GetAddAwakableCallCount());
  uint32_t result_index = static_cast<uint32_t>(-1);
  ASSERT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      core()->WaitMany(&h, &handle_signals, 1, MOJO_DEADLINE_INDEFINITE,
                       &result_index, nullptr));
  ASSERT_EQ(7u, info.GetAddAwakableCallCount());
  ASSERT_EQ(0u, result_index);
  hss = kFullMojoHandleSignalsState;
  ASSERT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      core()->WaitMany(&h, &handle_signals, 1, MOJO_DEADLINE_INDEFINITE,
                       nullptr, &hss));
  ASSERT_EQ(8u, info.GetAddAwakableCallCount());
  ASSERT_EQ(0u, hss.satisfied_signals);
  ASSERT_EQ(0u, hss.satisfiable_signals);
  result_index = static_cast<uint32_t>(-1);
  hss = kFullMojoHandleSignalsState;
  ASSERT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      core()->WaitMany(&h, &handle_signals, 1, MOJO_DEADLINE_INDEFINITE,
                       &result_index, &hss));
  ASSERT_EQ(9u, info.GetAddAwakableCallCount());
  ASSERT_EQ(0u, result_index);
  ASSERT_EQ(0u, hss.satisfied_signals);
  ASSERT_EQ(0u, hss.satisfiable_signals);

  ASSERT_EQ(0u, info.GetDtorCallCount());
  ASSERT_EQ(0u, info.GetCloseCallCount());
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h));
  ASSERT_EQ(1u, info.GetCloseCallCount());
  ASSERT_EQ(1u, info.GetDtorCallCount());

  // No awakables should ever have ever been added.
  ASSERT_EQ(0u, info.GetRemoveAwakableCallCount());
}

TEST_F(CoreTest, InvalidArguments) {
  // |Close()|:
  {
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(MOJO_HANDLE_INVALID));
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(10));
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(1000000000));

    // Test a double-close.
    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);
    ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h));
    ASSERT_EQ(1u, info.GetCloseCallCount());
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(h));
    ASSERT_EQ(1u, info.GetCloseCallCount());
  }

  // |Wait()|:
  {
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->Wait(MOJO_HANDLE_INVALID, ~MOJO_HANDLE_SIGNAL_NONE,
                           MOJO_DEADLINE_INDEFINITE, nullptr));
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->Wait(10, ~MOJO_HANDLE_SIGNAL_NONE,
                           MOJO_DEADLINE_INDEFINITE, nullptr));

    MojoHandleSignalsState hss = kFullMojoHandleSignalsState;
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->Wait(MOJO_HANDLE_INVALID, ~MOJO_HANDLE_SIGNAL_NONE,
                           MOJO_DEADLINE_INDEFINITE, &hss));
    // On invalid argument, it shouldn't modify the handle signals state.
    ASSERT_EQ(kFullMojoHandleSignalsState.satisfied_signals,
              hss.satisfied_signals);
    ASSERT_EQ(kFullMojoHandleSignalsState.satisfiable_signals,
              hss.satisfiable_signals);
    hss = kFullMojoHandleSignalsState;
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->Wait(10, ~MOJO_HANDLE_SIGNAL_NONE,
                           MOJO_DEADLINE_INDEFINITE, &hss));
    // On invalid argument, it shouldn't modify the handle signals state.
    ASSERT_EQ(kFullMojoHandleSignalsState.satisfied_signals,
              hss.satisfied_signals);
    ASSERT_EQ(kFullMojoHandleSignalsState.satisfiable_signals,
              hss.satisfiable_signals);
  }

  // |WaitMany()|:
  {
    MojoHandle handles[2] = {MOJO_HANDLE_INVALID, MOJO_HANDLE_INVALID};
    MojoHandleSignals signals[2] = {~MOJO_HANDLE_SIGNAL_NONE,
                                    ~MOJO_HANDLE_SIGNAL_NONE};
    ASSERT_EQ(
        MOJO_RESULT_INVALID_ARGUMENT,
        core()->WaitMany(handles, signals, 0, MOJO_DEADLINE_INDEFINITE,
                         nullptr, nullptr));
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(nullptr, signals, 0, MOJO_DEADLINE_INDEFINITE,
                               nullptr, nullptr));
    // If |num_handles| is invalid, it should leave |result_index| and
    // |signals_states| alone.
    // (We use -1 internally; make sure that doesn't leak.)
    uint32_t result_index = 123;
    MojoHandleSignalsState hss = kFullMojoHandleSignalsState;
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(nullptr, signals, 0, MOJO_DEADLINE_INDEFINITE,
                               &result_index, &hss));
    ASSERT_EQ(123u, result_index);
    ASSERT_EQ(kFullMojoHandleSignalsState.satisfied_signals,
              hss.satisfied_signals);
    ASSERT_EQ(kFullMojoHandleSignalsState.satisfiable_signals,
              hss.satisfiable_signals);

    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, nullptr, 0, MOJO_DEADLINE_INDEFINITE,
                               nullptr, nullptr));
    ASSERT_EQ(
        MOJO_RESULT_INVALID_ARGUMENT,
        core()->WaitMany(handles, signals, 1, MOJO_DEADLINE_INDEFINITE, nullptr,
                         nullptr));
    // But if a handle is bad, then it should set |result_index| but still leave
    // |signals_states| alone.
    result_index = static_cast<uint32_t>(-1);
    hss = kFullMojoHandleSignalsState;
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(
                  handles, signals, 1, MOJO_DEADLINE_INDEFINITE, &result_index,
                  &hss));
    ASSERT_EQ(0u, result_index);
    ASSERT_EQ(kFullMojoHandleSignalsState.satisfied_signals,
              hss.satisfied_signals);
    ASSERT_EQ(kFullMojoHandleSignalsState.satisfiable_signals,
              hss.satisfiable_signals);

    MockHandleInfo info[2];
    handles[0] = CreateMockHandle(&info[0]);

    result_index = static_cast<uint32_t>(-1);
    hss = kFullMojoHandleSignalsState;
    ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              core()->WaitMany(
                  handles, signals, 1, MOJO_DEADLINE_INDEFINITE, &result_index,
                  &hss));
    ASSERT_EQ(0u, result_index);
    ASSERT_EQ(0u, hss.satisfied_signals);
    ASSERT_EQ(0u, hss.satisfiable_signals);

    // On invalid argument, it'll leave |signals_states| alone.
    result_index = static_cast<uint32_t>(-1);
    hss = kFullMojoHandleSignalsState;
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(
                  handles, signals, 2, MOJO_DEADLINE_INDEFINITE, &result_index,
                  &hss));
    ASSERT_EQ(1u, result_index);
    ASSERT_EQ(kFullMojoHandleSignalsState.satisfied_signals,
              hss.satisfied_signals);
    ASSERT_EQ(kFullMojoHandleSignalsState.satisfiable_signals,
              hss.satisfiable_signals);
    handles[1] = handles[0] + 1;  // Invalid handle.
    ASSERT_EQ(
        MOJO_RESULT_INVALID_ARGUMENT,
        core()->WaitMany(handles, signals, 2, MOJO_DEADLINE_INDEFINITE, nullptr,
                         nullptr));
    handles[1] = CreateMockHandle(&info[1]);
    ASSERT_EQ(
        MOJO_RESULT_FAILED_PRECONDITION,
        core()->WaitMany(handles, signals, 2, MOJO_DEADLINE_INDEFINITE, nullptr,
                         nullptr));

    // TODO(vtl): Test one where we get "failed precondition" only for the
    // second handle (and the first one is valid to wait on).

    ASSERT_EQ(MOJO_RESULT_OK, core()->Close(handles[0]));
    ASSERT_EQ(MOJO_RESULT_OK, core()->Close(handles[1]));
  }

  // |CreateMessagePipe()|: Nothing to check (apart from things that cause
  // death).

  // |WriteMessage()|:
  // Only check arguments checked by |Core|, namely |handle|, |handles|, and
  // |num_handles|.
  {
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(MOJO_HANDLE_INVALID, nullptr, 0,
                                   nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));

    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);
    MojoHandle handles[2] = {MOJO_HANDLE_INVALID, MOJO_HANDLE_INVALID};

    // Huge handle count (implausibly big on some systems -- more than can be
    // stored in a 32-bit address space).
    // Note: This may return either |MOJO_RESULT_INVALID_ARGUMENT| or
    // |MOJO_RESULT_RESOURCE_EXHAUSTED|, depending on whether it's plausible or
    // not.
    ASSERT_NE(
        MOJO_RESULT_OK,
        core()->WriteMessage(h, nullptr, 0, handles,
                             std::numeric_limits<uint32_t>::max(),
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(0u, info.GetWriteMessageCallCount());

    // Null |bytes| with non-zero message size.
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(h, nullptr, 1, nullptr, 0,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(0u, info.GetWriteMessageCallCount());

    // Null |handles| with non-zero handle count.
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(h, nullptr, 0, nullptr, 1,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(0u, info.GetWriteMessageCallCount());

    // Huge handle count (plausibly big).
    ASSERT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
              core()->WriteMessage(
                  h, nullptr, 0, handles,
                  std::numeric_limits<uint32_t>::max() / sizeof(handles[0]),
                  MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(0u, info.GetWriteMessageCallCount());

    // Invalid handle in |handles|.
    ASSERT_EQ(
        MOJO_RESULT_INVALID_ARGUMENT,
        core()->WriteMessage(h, nullptr, 0, handles, 1,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(0u, info.GetWriteMessageCallCount());

    // Two invalid handles in |handles|.
    ASSERT_EQ(
        MOJO_RESULT_INVALID_ARGUMENT,
        core()->WriteMessage(h, nullptr, 0, handles, 2,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(0u, info.GetWriteMessageCallCount());

    // Can't send a handle over itself. Note that this will also cause |h| to be
    // closed.
    handles[0] = h;
    ASSERT_EQ(
        MOJO_RESULT_INVALID_ARGUMENT,
        core()->WriteMessage(h, nullptr, 0, handles, 1,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(0u, info.GetWriteMessageCallCount());

    h = CreateMockHandle(&info);

    MockHandleInfo info2;

    // This is "okay", but |MockDispatcher| doesn't implement it.
    handles[0] = CreateMockHandle(&info2);
    ASSERT_EQ(
        MOJO_RESULT_UNIMPLEMENTED,
        core()->WriteMessage(h, nullptr, 0, handles, 1,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(1u, info.GetWriteMessageCallCount());

    // One of the |handles| is still invalid.
    handles[0] = CreateMockHandle(&info2);
    ASSERT_EQ(
        MOJO_RESULT_INVALID_ARGUMENT,
        core()->WriteMessage(h, nullptr, 0, handles, 2,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(1u, info.GetWriteMessageCallCount());

    // One of the |handles| is the same as |h|. Both handles are closed.
    handles[0] = CreateMockHandle(&info2);
    handles[1] = h;
    ASSERT_EQ(
        MOJO_RESULT_INVALID_ARGUMENT,
        core()->WriteMessage(h, nullptr, 0, handles, 2,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(1u, info.GetWriteMessageCallCount());

    h = CreateMockHandle(&info);

    // Can't send a handle twice in the same message.
    handles[0] = CreateMockHandle(&info2);
    handles[1] = handles[0];
    ASSERT_EQ(
        MOJO_RESULT_BUSY,
        core()->WriteMessage(h, nullptr, 0, handles, 2,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(1u, info.GetWriteMessageCallCount());

    ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h));
  }

  // |ReadMessage()|:
  // Only check arguments checked by |Core|, namely |handle|, |handles|, and
  // |num_handles|.
  {
    ASSERT_EQ(
        MOJO_RESULT_INVALID_ARGUMENT,
        core()->ReadMessage(MOJO_HANDLE_INVALID, nullptr, nullptr, nullptr,
                            nullptr, MOJO_READ_MESSAGE_FLAG_NONE));

    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);

    // Okay.
    uint32_t handle_count = 0;
    ASSERT_EQ(MOJO_RESULT_OK,
              core()->ReadMessage(
                  h, nullptr, nullptr, nullptr, &handle_count,
                  MOJO_READ_MESSAGE_FLAG_NONE));
    // Checked by |Core|, shouldn't go through to the dispatcher.
    ASSERT_EQ(1u, info.GetReadMessageCallCount());

    ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h));
  }
}

// These test invalid arguments that should cause death if we're being paranoid
// about checking arguments (which we would want to do if, e.g., we were in a
// true "kernel" situation, but we might not want to do otherwise for
// performance reasons). Probably blatant errors like passing in null pointers
// (for required pointer arguments) will still cause death, but perhaps not
// predictably.
TEST_F(CoreTest, InvalidArgumentsDeath) {
#if defined(OFFICIAL_BUILD)
  const char kMemoryCheckFailedRegex[] = "";
#else
  const char kMemoryCheckFailedRegex[] = "Check failed";
#endif

  // |WaitMany()|:
  {
    MojoHandle handle = MOJO_HANDLE_INVALID;
    MojoHandleSignals signals = ~MOJO_HANDLE_SIGNAL_NONE;
    ASSERT_DEATH_IF_SUPPORTED(
        core()->WaitMany(nullptr, &signals, 1, MOJO_DEADLINE_INDEFINITE,
                         nullptr, nullptr),
        kMemoryCheckFailedRegex);
    ASSERT_DEATH_IF_SUPPORTED(
        core()->WaitMany(&handle, nullptr, 1, MOJO_DEADLINE_INDEFINITE, nullptr,
                         nullptr),
        kMemoryCheckFailedRegex);
    // TODO(vtl): |result_index| and |signals_states| are optional. Test them
    // with non-null invalid pointers?
  }

  // |CreateMessagePipe()|:
  {
    MojoHandle h;
    ASSERT_DEATH_IF_SUPPORTED(
        core()->CreateMessagePipe(nullptr, nullptr, nullptr),
        kMemoryCheckFailedRegex);
    ASSERT_DEATH_IF_SUPPORTED(
        core()->CreateMessagePipe(nullptr, &h, nullptr),
        kMemoryCheckFailedRegex);
    ASSERT_DEATH_IF_SUPPORTED(
        core()->CreateMessagePipe(nullptr, nullptr, &h),
        kMemoryCheckFailedRegex);
  }

  // |ReadMessage()|:
  // Only check arguments checked by |Core|, namely |handle|, |handles|, and
  // |num_handles|.
  {
    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);

    uint32_t handle_count = 1;
    ASSERT_DEATH_IF_SUPPORTED(
        core()->ReadMessage(h, nullptr, nullptr, nullptr, &handle_count,
                            MOJO_READ_MESSAGE_FLAG_NONE),
        kMemoryCheckFailedRegex);

    ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h));
  }
}

// TODO(vtl): test |Wait()| and |WaitMany()| properly
//  - including |WaitMany()| with the same handle more than once (with
//    same/different signals)

TEST_F(CoreTest, MessagePipe) {
  MojoHandle h[2];
  MojoHandleSignalsState hss[2];
  uint32_t result_index;

  ASSERT_EQ(MOJO_RESULT_OK, core()->CreateMessagePipe(nullptr, &h[0], &h[1]));
  // Should get two distinct, valid handles.
  ASSERT_NE(h[0], MOJO_HANDLE_INVALID);
  ASSERT_NE(h[1], MOJO_HANDLE_INVALID);
  ASSERT_NE(h[0], h[1]);

  // Neither should be readable.
  MojoHandleSignals signals[2] = {MOJO_HANDLE_SIGNAL_READABLE,
                                  MOJO_HANDLE_SIGNAL_READABLE};
  result_index = static_cast<uint32_t>(-1);
  hss[0] = kEmptyMojoHandleSignalsState;
  hss[1] = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(
      MOJO_RESULT_DEADLINE_EXCEEDED,
      core()->WaitMany(h, signals, 2, 0, &result_index, hss));
  ASSERT_EQ(static_cast<uint32_t>(-1), result_index);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss[0].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[0].satisfiable_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss[1].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[1].satisfiable_signals);

  // Try to read anyway.
  char buffer[1] = {'a'};
  uint32_t buffer_size = 1;
  ASSERT_EQ(
      MOJO_RESULT_SHOULD_WAIT,
      core()->ReadMessage(h[0], buffer, &buffer_size, nullptr, nullptr,
                          MOJO_READ_MESSAGE_FLAG_NONE));
  // Check that it left its inputs alone.
  ASSERT_EQ('a', buffer[0]);
  ASSERT_EQ(1u, buffer_size);

  // Both should be writable.
  hss[0] = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK, core()->Wait(h[0], MOJO_HANDLE_SIGNAL_WRITABLE,
                                         1000000000, &hss[0]));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss[0].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[0].satisfiable_signals);
  hss[0] = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK, core()->Wait(h[1], MOJO_HANDLE_SIGNAL_WRITABLE,
                                         1000000000, &hss[0]));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss[0].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[0].satisfiable_signals);

  // Also check that |h[1]| is writable using |WaitMany()|.
  signals[0] = MOJO_HANDLE_SIGNAL_READABLE;
  signals[1] = MOJO_HANDLE_SIGNAL_WRITABLE;
  result_index = static_cast<uint32_t>(-1);
  hss[0] = kEmptyMojoHandleSignalsState;
  hss[1] = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      core()->WaitMany(h, signals, 2, MOJO_DEADLINE_INDEFINITE, &result_index,
                       hss));
  ASSERT_EQ(1u, result_index);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss[0].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[0].satisfiable_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss[1].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[1].satisfiable_signals);

  // Write to |h[1]|.
  buffer[0] = 'b';
  ASSERT_EQ(
      MOJO_RESULT_OK,
      core()->WriteMessage(h[1], buffer, 1, nullptr, 0,
                           MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Check that |h[0]| is now readable.
  signals[0] = MOJO_HANDLE_SIGNAL_READABLE;
  signals[1] = MOJO_HANDLE_SIGNAL_READABLE;
  result_index = static_cast<uint32_t>(-1);
  hss[0] = kEmptyMojoHandleSignalsState;
  hss[1] = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      core()->WaitMany(h, signals, 2, MOJO_DEADLINE_INDEFINITE, &result_index,
                       hss));
  ASSERT_EQ(0u, result_index);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss[0].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[0].satisfiable_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss[1].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[1].satisfiable_signals);

  // Read from |h[0]|.
  // First, get only the size.
  buffer_size = 0;
  ASSERT_EQ(
      MOJO_RESULT_RESOURCE_EXHAUSTED,
      core()->ReadMessage(h[0], nullptr, &buffer_size, nullptr, nullptr,
                          MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(1u, buffer_size);
  // Then actually read it.
  buffer[0] = 'c';
  buffer_size = 1;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      core()->ReadMessage(h[0], buffer, &buffer_size, nullptr, nullptr,
                          MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ('b', buffer[0]);
  ASSERT_EQ(1u, buffer_size);

  // |h[0]| should no longer be readable.
  hss[0] = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            core()->Wait(h[0], MOJO_HANDLE_SIGNAL_READABLE, 0, &hss[0]));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss[0].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[0].satisfiable_signals);

  // Write to |h[0]|.
  buffer[0] = 'd';
  ASSERT_EQ(
      MOJO_RESULT_OK,
      core()->WriteMessage(h[0], buffer, 1, nullptr, 0,
                           MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Close |h[0]|.
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h[0]));

  // Wait for |h[1]| to learn about the other end's closure.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(h[1], MOJO_HANDLE_SIGNAL_PEER_CLOSED, 1000000000,
                         &hss[0]));

  // Check that |h[1]| is no longer writable (and will never be).
  hss[0] = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h[1], MOJO_HANDLE_SIGNAL_WRITABLE, 1000000000,
                         &hss[0]));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss[0].satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss[0].satisfiable_signals);

  // Check that |h[1]| is still readable (for the moment).
  hss[0] = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK, core()->Wait(h[1], MOJO_HANDLE_SIGNAL_READABLE,
                                         1000000000, &hss[0]));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss[0].satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss[0].satisfiable_signals);

  // Discard a message from |h[1]|.
  ASSERT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            core()->ReadMessage(h[1], nullptr, nullptr, nullptr, nullptr,
                                MOJO_READ_MESSAGE_FLAG_MAY_DISCARD));

  // |h[1]| is no longer readable (and will never be).
  hss[0] = kFullMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h[1], MOJO_HANDLE_SIGNAL_READABLE, 1000000000,
                         &hss[0]));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss[0].satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss[0].satisfiable_signals);

  // Try writing to |h[1]|.
  buffer[0] = 'e';
  ASSERT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      core()->WriteMessage(h[1], buffer, 1, nullptr, 0,
                           MOJO_WRITE_MESSAGE_FLAG_NONE));

  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h[1]));
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
  MojoHandleSignalsState hss;
  MojoHandle h_received;

  MojoHandle h_passing[2];
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->CreateMessagePipe(nullptr, &h_passing[0], &h_passing[1]));

  // Make sure that |h_passing[]| work properly.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0], kHello, kHelloSize, nullptr, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE, 1000000000,
                         &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(
                h_passing[1], buffer, &num_bytes, handles, &num_handles,
                MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(kHelloSize, num_bytes);
  ASSERT_STREQ(kHello, buffer);
  ASSERT_EQ(0u, num_handles);

  // Make sure that you can't pass either of the message pipe's handles over
  // itself.
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            core()->WriteMessage(h_passing[0], kHello, kHelloSize,
                                 &h_passing[0], 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->CreateMessagePipe(nullptr, &h_passing[0], &h_passing[1]));

  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            core()->WriteMessage(h_passing[0], kHello, kHelloSize,
                                 &h_passing[1], 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->CreateMessagePipe(nullptr, &h_passing[0], &h_passing[1]));

  MojoHandle h_passed[2];
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->CreateMessagePipe(nullptr, &h_passed[0], &h_passed[1]));

  // Make sure that |h_passed[]| work properly.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passed[0], kHello, kHelloSize, nullptr, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passed[1], MOJO_HANDLE_SIGNAL_READABLE, 1000000000,
                         &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(
                h_passed[1], buffer, &num_bytes, handles, &num_handles,
                MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(kHelloSize, num_bytes);
  ASSERT_STREQ(kHello, buffer);
  ASSERT_EQ(0u, num_handles);

  // Send |h_passed[1]| from |h_passing[0]| to |h_passing[1]|.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0], kWorld, kWorldSize,
                                 &h_passed[1], 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE, 1000000000,
                         &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(
                h_passing[1], buffer, &num_bytes, handles, &num_handles,
                MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(kWorldSize, num_bytes);
  ASSERT_STREQ(kWorld, buffer);
  ASSERT_EQ(1u, num_handles);
  h_received = handles[0];
  ASSERT_NE(h_received, MOJO_HANDLE_INVALID);
  ASSERT_NE(h_received, h_passing[0]);
  ASSERT_NE(h_received, h_passing[1]);
  ASSERT_NE(h_received, h_passed[0]);

  // Note: We rely on the Mojo system not re-using handle values very often.
  ASSERT_NE(h_received, h_passed[1]);

  // |h_passed[1]| should no longer be valid; check that trying to close it
  // fails. See above note.
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(h_passed[1]));

  // Write to |h_passed[0]|. Should receive on |h_received|.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passed[0], kHello, kHelloSize, nullptr, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_received, MOJO_HANDLE_SIGNAL_READABLE, 1000000000,
                         &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(
                h_received, buffer, &num_bytes, handles, &num_handles,
                MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(kHelloSize, num_bytes);
  ASSERT_STREQ(kHello, buffer);
  ASSERT_EQ(0u, num_handles);

  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[0]));
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[1]));
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h_passed[0]));
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h_received));
}

TEST_F(CoreTest, DataPipe) {
  MojoHandle ph, ch;  // p is for producer and c is for consumer.
  MojoHandleSignalsState hss;

  ASSERT_EQ(MOJO_RESULT_OK,
            core()->CreateDataPipe(nullptr, &ph, &ch));
  // Should get two distinct, valid handles.
  ASSERT_NE(ph, MOJO_HANDLE_INVALID);
  ASSERT_NE(ch, MOJO_HANDLE_INVALID);
  ASSERT_NE(ph, ch);

  // Producer should be never-readable, but already writable.
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      core()->Wait(ph, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK, core()->Wait(ph, MOJO_HANDLE_SIGNAL_WRITABLE, 0,
                                         &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Consumer should be never-writable, and not yet readable.
  hss = kFullMojoHandleSignalsState;
  ASSERT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      core()->Wait(ch, MOJO_HANDLE_SIGNAL_WRITABLE, 0, &hss));
  ASSERT_EQ(0u, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);
  hss = kFullMojoHandleSignalsState;
  ASSERT_EQ(
      MOJO_RESULT_DEADLINE_EXCEEDED,
      core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  ASSERT_EQ(0u, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Write.
  signed char elements[2] = {'A', 'B'};
  uint32_t num_bytes = 2u;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteData(ph, elements, &num_bytes,
                              MOJO_WRITE_DATA_FLAG_NONE));
  ASSERT_EQ(2u, num_bytes);

  // Wait for the data to arrive to the consumer.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 1000000000, &hss));

  // Consumer should now be readable.
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK, core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 0,
                                         &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Peek one character.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = 1u;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadData(
                ch, elements, &num_bytes,
                MOJO_READ_DATA_FLAG_NONE | MOJO_READ_DATA_FLAG_PEEK));
  ASSERT_EQ('A', elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Read one character.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = 1u;
  ASSERT_EQ(MOJO_RESULT_OK, core()->ReadData(ch, elements, &num_bytes,
                                             MOJO_READ_DATA_FLAG_NONE));
  ASSERT_EQ('A', elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Two-phase write.
  void* write_ptr = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->BeginWriteData(ph, &write_ptr, &num_bytes,
                                   MOJO_WRITE_DATA_FLAG_NONE));
  // We count on the default options providing a decent buffer size.
  ASSERT_GE(num_bytes, 3u);

  // Trying to do a normal write during a two-phase write should fail.
  elements[0] = 'X';
  num_bytes = 1u;
  ASSERT_EQ(MOJO_RESULT_BUSY,
            core()->WriteData(ph, elements, &num_bytes,
                              MOJO_WRITE_DATA_FLAG_NONE));

  // Actually write the data, and complete it now.
  static_cast<char*>(write_ptr)[0] = 'C';
  static_cast<char*>(write_ptr)[1] = 'D';
  static_cast<char*>(write_ptr)[2] = 'E';
  ASSERT_EQ(MOJO_RESULT_OK, core()->EndWriteData(ph, 3u));

  // Wait for the data to arrive to the consumer.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 1000000000, &hss));

  // Query how much data we have.
  num_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch, nullptr, &num_bytes,
                             MOJO_READ_DATA_FLAG_QUERY));
  ASSERT_GE(num_bytes, 1u);

  // Try to query with peek. Should fail.
  num_bytes = 0;
  ASSERT_EQ(
      MOJO_RESULT_INVALID_ARGUMENT,
      core()->ReadData(ch, nullptr, &num_bytes,
                       MOJO_READ_DATA_FLAG_QUERY | MOJO_READ_DATA_FLAG_PEEK));
  ASSERT_EQ(0u, num_bytes);

  // Try to discard ten characters, in all-or-none mode. Should fail.
  num_bytes = 10;
  ASSERT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            core()->ReadData(
                ch, nullptr, &num_bytes,
                MOJO_READ_DATA_FLAG_DISCARD | MOJO_READ_DATA_FLAG_ALL_OR_NONE));

  // Try to discard two characters, in peek mode. Should fail.
  num_bytes = 2;
  ASSERT_EQ(
      MOJO_RESULT_INVALID_ARGUMENT,
      core()->ReadData(ch, nullptr, &num_bytes,
                       MOJO_READ_DATA_FLAG_DISCARD | MOJO_READ_DATA_FLAG_PEEK));

  // Discard a character.
  num_bytes = 1;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadData(
                ch, nullptr, &num_bytes,
                MOJO_READ_DATA_FLAG_DISCARD | MOJO_READ_DATA_FLAG_ALL_OR_NONE));

  // Ensure the 3 bytes were read.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 1000000000, &hss));

  // Try a two-phase read of the remaining three bytes with peek. Should fail.
  const void* read_ptr = nullptr;
  num_bytes = 3;
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            core()->BeginReadData(ch, &read_ptr, &num_bytes,
                                  MOJO_READ_DATA_FLAG_PEEK));

  // Read the remaining two characters, in two-phase mode (all-or-none).
  num_bytes = 3;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->BeginReadData(ch, &read_ptr, &num_bytes,
                                  MOJO_READ_DATA_FLAG_ALL_OR_NONE));
  // Note: Count on still being able to do the contiguous read here.
  ASSERT_EQ(3u, num_bytes);

  // Discarding right now should fail.
  num_bytes = 1;
  ASSERT_EQ(MOJO_RESULT_BUSY,
            core()->ReadData(ch, nullptr, &num_bytes,
                             MOJO_READ_DATA_FLAG_DISCARD));

  // Actually check our data and end the two-phase read.
  ASSERT_EQ('C', static_cast<const char*>(read_ptr)[0]);
  ASSERT_EQ('D', static_cast<const char*>(read_ptr)[1]);
  ASSERT_EQ('E', static_cast<const char*>(read_ptr)[2]);
  ASSERT_EQ(MOJO_RESULT_OK, core()->EndReadData(ch, 3u));

  // Consumer should now be no longer readable.
  hss = kFullMojoHandleSignalsState;
  ASSERT_EQ(
      MOJO_RESULT_DEADLINE_EXCEEDED,
      core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  ASSERT_EQ(0u, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // TODO(vtl): More.

  // Close the producer.
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(ph));

  // Wait for this to get to the consumer.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(ch, MOJO_HANDLE_SIGNAL_PEER_CLOSED, 1000000000, &hss));

  // The consumer should now be never-readable.
  hss = kFullMojoHandleSignalsState;
  ASSERT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);

  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(ch));
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
  MojoHandleSignalsState hss;

  MojoHandle h_passing[2];
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->CreateMessagePipe(nullptr, &h_passing[0], &h_passing[1]));

  MojoHandle ph, ch;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->CreateDataPipe(nullptr, &ph, &ch));

  // Send |ch| from |h_passing[0]| to |h_passing[1]|.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0], kHello, kHelloSize, &ch, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE, 1000000000,
                         &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(
                h_passing[1], buffer, &num_bytes, handles, &num_handles,
                MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(kHelloSize, num_bytes);
  ASSERT_STREQ(kHello, buffer);
  ASSERT_EQ(1u, num_handles);
  MojoHandle ch_received = handles[0];
  ASSERT_NE(ch_received, MOJO_HANDLE_INVALID);
  ASSERT_NE(ch_received, h_passing[0]);
  ASSERT_NE(ch_received, h_passing[1]);
  ASSERT_NE(ch_received, ph);

  // Note: We rely on the Mojo system not re-using handle values very often.
  ASSERT_NE(ch_received, ch);

  // |ch| should no longer be valid; check that trying to close it fails. See
  // above note.
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(ch));

  // Write to |ph|. Should receive on |ch_received|.
  num_bytes = kWorldSize;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteData(ph, kWorld, &num_bytes,
                              MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(ch_received, MOJO_HANDLE_SIGNAL_READABLE, 1000000000,
                         &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);
  num_bytes = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch_received, buffer, &num_bytes,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(kWorldSize, num_bytes);
  ASSERT_STREQ(kWorld, buffer);

  // Now pass |ph| in the same direction.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0], kWorld, kWorldSize, &ph, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE, 1000000000,
                         &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(
                h_passing[1], buffer, &num_bytes, handles, &num_handles,
                MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(kWorldSize, num_bytes);
  ASSERT_STREQ(kWorld, buffer);
  ASSERT_EQ(1u, num_handles);
  MojoHandle ph_received = handles[0];
  ASSERT_NE(ph_received, MOJO_HANDLE_INVALID);
  ASSERT_NE(ph_received, h_passing[0]);
  ASSERT_NE(ph_received, h_passing[1]);
  ASSERT_NE(ph_received, ch_received);

  // Again, rely on the Mojo system not re-using handle values very often.
  ASSERT_NE(ph_received, ph);

  // |ph| should no longer be valid; check that trying to close it fails. See
  // above note.
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(ph));

  // Write to |ph_received|. Should receive on |ch_received|.
  num_bytes = kHelloSize;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteData(ph_received, kHello, &num_bytes,
                              MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(ch_received, MOJO_HANDLE_SIGNAL_READABLE, 1000000000,
                         &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);
  num_bytes = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch_received, buffer, &num_bytes,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(kHelloSize, num_bytes);
  ASSERT_STREQ(kHello, buffer);

  ph = ph_received;
  ph_received = MOJO_HANDLE_INVALID;
  ch = ch_received;
  ch_received = MOJO_HANDLE_INVALID;

  // Make sure that |ph| can't be sent if it's in a two-phase write.
  void* write_ptr = nullptr;
  num_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->BeginWriteData(ph, &write_ptr, &num_bytes,
                                   MOJO_WRITE_DATA_FLAG_NONE));
  ASSERT_GE(num_bytes, 1u);
  ASSERT_EQ(MOJO_RESULT_BUSY,
            core()->WriteMessage(h_passing[0], kHello, kHelloSize, &ph, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));

  // But |ch| can, even if |ph| is in a two-phase write.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0], kHello, kHelloSize, &ch, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  ch = MOJO_HANDLE_INVALID;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE, 1000000000,
                         nullptr));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(
                h_passing[1], buffer, &num_bytes, handles, &num_handles,
                MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(kHelloSize, num_bytes);
  ASSERT_STREQ(kHello, buffer);
  ASSERT_EQ(1u, num_handles);
  ch = handles[0];
  ASSERT_NE(ch, MOJO_HANDLE_INVALID);

  // Complete the two-phase write.
  static_cast<char*>(write_ptr)[0] = 'x';
  ASSERT_EQ(MOJO_RESULT_OK, core()->EndWriteData(ph, 1));

  // Wait for |ch| to be readable.
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK, core()->Wait(ch, MOJO_HANDLE_SIGNAL_READABLE,
                                         1000000000, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Make sure that |ch| can't be sent if it's in a two-phase read.
  const void* read_ptr = nullptr;
  num_bytes = 1;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->BeginReadData(ch, &read_ptr, &num_bytes,
                                  MOJO_READ_DATA_FLAG_ALL_OR_NONE));
  ASSERT_EQ(MOJO_RESULT_BUSY,
            core()->WriteMessage(h_passing[0], kHello, kHelloSize, &ch, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));

  // But |ph| can, even if |ch| is in a two-phase read.
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0], kWorld, kWorldSize, &ph, 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  ph = MOJO_HANDLE_INVALID;
  hss = kEmptyMojoHandleSignalsState;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_HANDLE_SIGNAL_READABLE, 1000000000,
                         &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(
                h_passing[1], buffer, &num_bytes, handles, &num_handles,
                MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(kWorldSize, num_bytes);
  ASSERT_STREQ(kWorld, buffer);
  ASSERT_EQ(1u, num_handles);
  ph = handles[0];
  ASSERT_NE(ph, MOJO_HANDLE_INVALID);

  // Complete the two-phase read.
  ASSERT_EQ('x', static_cast<const char*>(read_ptr)[0]);
  ASSERT_EQ(MOJO_RESULT_OK, core()->EndReadData(ch, 1));

  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[0]));
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[1]));
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(ph));
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(ch));
}

struct TestAsyncWaiter {
  TestAsyncWaiter() : result(MOJO_RESULT_UNKNOWN) {}

  void Awake(MojoResult r) { result = r; }

  MojoResult result;
};

// TODO(vtl): Test |DuplicateBufferHandle()| and |MapBuffer()|.

}  // namespace
}  // namespace edk
}  // namespace mojo
