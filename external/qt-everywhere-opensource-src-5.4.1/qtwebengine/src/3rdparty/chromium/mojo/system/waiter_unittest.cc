// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE(vtl): Some of these tests are inherently flaky (e.g., if run on a
// heavily-loaded system). Sorry. |test::EpsilonTimeout()| may be increased to
// increase tolerance and reduce observed flakiness (though doing so reduces the
// meaningfulness of the test).

#include "mojo/system/waiter.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "mojo/system/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

const int64_t kMicrosPerMs = 1000;
const int64_t kPollTimeMicros = 10 * kMicrosPerMs;  // 10 ms.

class WaitingThread : public base::SimpleThread {
 public:
  explicit WaitingThread(MojoDeadline deadline)
      : base::SimpleThread("waiting_thread"),
        deadline_(deadline),
        done_(false),
        result_(MOJO_RESULT_UNKNOWN),
        context_(static_cast<uint32_t>(-1)) {
    waiter_.Init();
  }

  virtual ~WaitingThread() {
    Join();
  }

  void WaitUntilDone(MojoResult* result,
                     uint32_t* context,
                     base::TimeDelta* elapsed) {
    for (;;) {
      {
        base::AutoLock locker(lock_);
        if (done_) {
          *result = result_;
          *context = context_;
          *elapsed = elapsed_;
          break;
        }
      }

      base::PlatformThread::Sleep(
          base::TimeDelta::FromMicroseconds(kPollTimeMicros));
    }
  }

  Waiter* waiter() { return &waiter_; }

 private:
  virtual void Run() OVERRIDE {
    test::Stopwatch stopwatch;
    MojoResult result;
    uint32_t context = static_cast<uint32_t>(-1);
    base::TimeDelta elapsed;

    stopwatch.Start();
    result = waiter_.Wait(deadline_, &context);
    elapsed = stopwatch.Elapsed();

    {
      base::AutoLock locker(lock_);
      done_ = true;
      result_ = result;
      context_ = context;
      elapsed_ = elapsed;
    }
  }

  const MojoDeadline deadline_;
  Waiter waiter_;  // Thread-safe.

  base::Lock lock_;  // Protects the following members.
  bool done_;
  MojoResult result_;
  uint32_t context_;
  base::TimeDelta elapsed_;

  DISALLOW_COPY_AND_ASSIGN(WaitingThread);
};

TEST(WaiterTest, Basic) {
  MojoResult result;
  uint32_t context;
  base::TimeDelta elapsed;

  // Finite deadline.

  // Awake immediately after thread start.
  {
    WaitingThread thread(10 * test::EpsilonTimeout().InMicroseconds());
    thread.Start();
    thread.waiter()->Awake(MOJO_RESULT_OK, 1);
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(1u, context);
    EXPECT_LT(elapsed, test::EpsilonTimeout());
  }

  // Awake before after thread start.
  {
    WaitingThread thread(10 * test::EpsilonTimeout().InMicroseconds());
    thread.waiter()->Awake(MOJO_RESULT_CANCELLED, 2);
    thread.Start();
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
    EXPECT_EQ(2u, context);
    EXPECT_LT(elapsed, test::EpsilonTimeout());
  }

  // Awake some time after thread start.
  {
    WaitingThread thread(10 * test::EpsilonTimeout().InMicroseconds());
    thread.Start();
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    thread.waiter()->Awake(1, 3);
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(1, result);
    EXPECT_EQ(3u, context);
    EXPECT_GT(elapsed, (2-1) * test::EpsilonTimeout());
    EXPECT_LT(elapsed, (2+1) * test::EpsilonTimeout());
  }

  // Awake some longer time after thread start.
  {
    WaitingThread thread(10 * test::EpsilonTimeout().InMicroseconds());
    thread.Start();
    base::PlatformThread::Sleep(5 * test::EpsilonTimeout());
    thread.waiter()->Awake(2, 4);
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(2, result);
    EXPECT_EQ(4u, context);
    EXPECT_GT(elapsed, (5-1) * test::EpsilonTimeout());
    EXPECT_LT(elapsed, (5+1) * test::EpsilonTimeout());
  }

  // Don't awake -- time out (on another thread).
  {
    WaitingThread thread(2 * test::EpsilonTimeout().InMicroseconds());
    thread.Start();
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, result);
    EXPECT_EQ(static_cast<uint32_t>(-1), context);
    EXPECT_GT(elapsed, (2-1) * test::EpsilonTimeout());
    EXPECT_LT(elapsed, (2+1) * test::EpsilonTimeout());
  }

  // No (indefinite) deadline.

  // Awake immediately after thread start.
  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.Start();
    thread.waiter()->Awake(MOJO_RESULT_OK, 5);
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(5u, context);
    EXPECT_LT(elapsed, test::EpsilonTimeout());
  }

  // Awake before after thread start.
  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.waiter()->Awake(MOJO_RESULT_CANCELLED, 6);
    thread.Start();
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
    EXPECT_EQ(6u, context);
    EXPECT_LT(elapsed, test::EpsilonTimeout());
  }

  // Awake some time after thread start.
  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.Start();
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    thread.waiter()->Awake(1, 7);
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(1, result);
    EXPECT_EQ(7u, context);
    EXPECT_GT(elapsed, (2-1) * test::EpsilonTimeout());
    EXPECT_LT(elapsed, (2+1) * test::EpsilonTimeout());
  }

  // Awake some longer time after thread start.
  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.Start();
    base::PlatformThread::Sleep(5 * test::EpsilonTimeout());
    thread.waiter()->Awake(2, 8);
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(2, result);
    EXPECT_EQ(8u, context);
    EXPECT_GT(elapsed, (5-1) * test::EpsilonTimeout());
    EXPECT_LT(elapsed, (5+1) * test::EpsilonTimeout());
  }
}

TEST(WaiterTest, TimeOut) {
  test::Stopwatch stopwatch;
  base::TimeDelta elapsed;

  Waiter waiter;
  uint32_t context = 123;

  waiter.Init();
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, &context));
  elapsed = stopwatch.Elapsed();
  EXPECT_LT(elapsed, test::EpsilonTimeout());
  EXPECT_EQ(123u, context);

  waiter.Init();
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            waiter.Wait(2 * test::EpsilonTimeout().InMicroseconds(), &context));
  elapsed = stopwatch.Elapsed();
  EXPECT_GT(elapsed, (2-1) * test::EpsilonTimeout());
  EXPECT_LT(elapsed, (2+1) * test::EpsilonTimeout());
  EXPECT_EQ(123u, context);

  waiter.Init();
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            waiter.Wait(5 * test::EpsilonTimeout().InMicroseconds(), &context));
  elapsed = stopwatch.Elapsed();
  EXPECT_GT(elapsed, (5-1) * test::EpsilonTimeout());
  EXPECT_LT(elapsed, (5+1) * test::EpsilonTimeout());
  EXPECT_EQ(123u, context);
}

// The first |Awake()| should always win.
TEST(WaiterTest, MultipleAwakes) {
  MojoResult result;
  uint32_t context;
  base::TimeDelta elapsed;

  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.Start();
    thread.waiter()->Awake(MOJO_RESULT_OK, 1);
    thread.waiter()->Awake(1, 2);
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(1u, context);
    EXPECT_LT(elapsed, test::EpsilonTimeout());
  }

  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.waiter()->Awake(1, 3);
    thread.Start();
    thread.waiter()->Awake(MOJO_RESULT_OK, 4);
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(1, result);
    EXPECT_EQ(3u, context);
    EXPECT_LT(elapsed, test::EpsilonTimeout());
  }

  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.Start();
    thread.waiter()->Awake(10, 5);
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    thread.waiter()->Awake(20, 6);
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(10, result);
    EXPECT_EQ(5u, context);
    EXPECT_LT(elapsed, test::EpsilonTimeout());
  }

  {
    WaitingThread thread(10 * test::EpsilonTimeout().InMicroseconds());
    thread.Start();
    base::PlatformThread::Sleep(1 * test::EpsilonTimeout());
    thread.waiter()->Awake(MOJO_RESULT_FAILED_PRECONDITION, 7);
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    thread.waiter()->Awake(MOJO_RESULT_OK, 8);
    thread.WaitUntilDone(&result, &context, &elapsed);
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
    EXPECT_EQ(7u, context);
    EXPECT_GT(elapsed, (1-1) * test::EpsilonTimeout());
    EXPECT_LT(elapsed, (1+1) * test::EpsilonTimeout());
  }
}

}  // namespace
}  // namespace system
}  // namespace mojo
