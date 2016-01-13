// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE(vtl): Some of these tests are inherently flaky (e.g., if run on a
// heavily-loaded system). Sorry. |test::EpsilonTimeout()| may be increased to
// increase tolerance and reduce observed flakiness (though doing so reduces the
// meaningfulness of the test).

#include "mojo/system/simple_dispatcher.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "base/time/time.h"
#include "mojo/system/test_utils.h"
#include "mojo/system/waiter.h"
#include "mojo/system/waiter_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

class MockSimpleDispatcher : public SimpleDispatcher {
 public:
  MockSimpleDispatcher()
      : state_(MOJO_HANDLE_SIGNAL_NONE,
               MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE) {}

  void SetSatisfiedSignals(MojoHandleSignals new_satisfied_signals) {
    base::AutoLock locker(lock());

    // Any new signals that are set should be satisfiable.
    CHECK_EQ(new_satisfied_signals & ~state_.satisfied_signals,
             new_satisfied_signals & ~state_.satisfied_signals &
                 state_.satisfiable_signals);

    if (new_satisfied_signals == state_.satisfied_signals)
      return;

    state_.satisfied_signals = new_satisfied_signals;
    HandleSignalsStateChangedNoLock();
  }

  void SetSatisfiableSignals(MojoHandleSignals new_satisfiable_signals) {
    base::AutoLock locker(lock());

    // Satisfied implies satisfiable.
    CHECK_EQ(new_satisfiable_signals & state_.satisfied_signals,
             state_.satisfied_signals);

    if (new_satisfiable_signals == state_.satisfiable_signals)
      return;

    state_.satisfiable_signals = new_satisfiable_signals;
    HandleSignalsStateChangedNoLock();
  }

  virtual Type GetType() const OVERRIDE {
    return kTypeUnknown;
  }

 private:
  friend class base::RefCountedThreadSafe<MockSimpleDispatcher>;
  virtual ~MockSimpleDispatcher() {}

  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() OVERRIDE {
    scoped_refptr<MockSimpleDispatcher> rv(new MockSimpleDispatcher());
    rv->state_ = state_;
    return scoped_refptr<Dispatcher>(rv.get());
  }

  // |SimpleDispatcher| implementation:
  virtual HandleSignalsState GetHandleSignalsStateNoLock() const OVERRIDE {
    lock().AssertAcquired();
    return state_;
  }

  // Protected by |lock()|:
  HandleSignalsState state_;

  DISALLOW_COPY_AND_ASSIGN(MockSimpleDispatcher);
};

TEST(SimpleDispatcherTest, Basic) {
  test::Stopwatch stopwatch;

  scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
  Waiter w;
  uint32_t context = 0;

  // Try adding a readable waiter when already readable.
  w.Init();
  d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_READABLE, 0));
  // Shouldn't need to remove the waiter (it was not added).

  // Wait (forever) for writable when already writable.
  w.Init();
  d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 1));
  d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_WRITABLE);
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_OK, w.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_LT(stopwatch.Elapsed(), test::EpsilonTimeout());
  EXPECT_EQ(1u, context);
  d->RemoveWaiter(&w);

  // Wait for zero time for writable when already writable.
  w.Init();
  d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 2));
  d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_WRITABLE);
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_OK, w.Wait(0, &context));
  EXPECT_LT(stopwatch.Elapsed(), test::EpsilonTimeout());
  EXPECT_EQ(2u, context);
  d->RemoveWaiter(&w);

  // Wait for non-zero, finite time for writable when already writable.
  w.Init();
  d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 3));
  d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_WRITABLE);
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_OK,
            w.Wait(2 * test::EpsilonTimeout().InMicroseconds(), &context));
  EXPECT_LT(stopwatch.Elapsed(), test::EpsilonTimeout());
  EXPECT_EQ(3u, context);
  d->RemoveWaiter(&w);

  // Wait for zero time for writable when not writable (will time out).
  w.Init();
  d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 4));
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, w.Wait(0, NULL));
  EXPECT_LT(stopwatch.Elapsed(), test::EpsilonTimeout());
  d->RemoveWaiter(&w);

  // Wait for non-zero, finite time for writable when not writable (will time
  // out).
  w.Init();
  d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 5));
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            w.Wait(2 * test::EpsilonTimeout().InMicroseconds(), NULL));
  base::TimeDelta elapsed = stopwatch.Elapsed();
  EXPECT_GT(elapsed, (2-1) * test::EpsilonTimeout());
  EXPECT_LT(elapsed, (2+1) * test::EpsilonTimeout());
  d->RemoveWaiter(&w);

  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
}

TEST(SimpleDispatcherTest, BasicUnsatisfiable) {
  test::Stopwatch stopwatch;

  scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
  Waiter w;
  uint32_t context = 0;

  // Try adding a writable waiter when it can never be writable.
  w.Init();
  d->SetSatisfiableSignals(MOJO_HANDLE_SIGNAL_READABLE);
  d->SetSatisfiedSignals(0);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 1));
  // Shouldn't need to remove the waiter (it was not added).

  // Wait (forever) for writable and then it becomes never writable.
  w.Init();
  d->SetSatisfiableSignals(
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 2));
  d->SetSatisfiableSignals(MOJO_HANDLE_SIGNAL_READABLE);
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            w.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_LT(stopwatch.Elapsed(), test::EpsilonTimeout());
  EXPECT_EQ(2u, context);
  d->RemoveWaiter(&w);

  // Wait for zero time for writable and then it becomes never writable.
  w.Init();
  d->SetSatisfiableSignals(
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 3));
  d->SetSatisfiableSignals(MOJO_HANDLE_SIGNAL_READABLE);
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, w.Wait(0, &context));
  EXPECT_LT(stopwatch.Elapsed(), test::EpsilonTimeout());
  EXPECT_EQ(3u, context);
  d->RemoveWaiter(&w);

  // Wait for non-zero, finite time for writable and then it becomes never
  // writable.
  w.Init();
  d->SetSatisfiableSignals(
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 4));
  d->SetSatisfiableSignals(MOJO_HANDLE_SIGNAL_READABLE);
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            w.Wait(2 * test::EpsilonTimeout().InMicroseconds(), &context));
  EXPECT_LT(stopwatch.Elapsed(), test::EpsilonTimeout());
  EXPECT_EQ(4u, context);
  d->RemoveWaiter(&w);

  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
}

TEST(SimpleDispatcherTest, BasicClosed) {
  test::Stopwatch stopwatch;

  scoped_refptr<MockSimpleDispatcher> d;
  Waiter w;
  uint32_t context = 0;

  // Try adding a writable waiter when the dispatcher has been closed.
  d = new MockSimpleDispatcher();
  w.Init();
  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 1));
  // Shouldn't need to remove the waiter (it was not added).

  // Wait (forever) for writable and then the dispatcher is closed.
  d = new MockSimpleDispatcher();
  w.Init();
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 2));
  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_CANCELLED, w.Wait(MOJO_DEADLINE_INDEFINITE, &context));
  EXPECT_LT(stopwatch.Elapsed(), test::EpsilonTimeout());
  EXPECT_EQ(2u, context);
  // Don't need to remove waiters from closed dispatchers.

  // Wait for zero time for writable and then the dispatcher is closed.
  d = new MockSimpleDispatcher();
  w.Init();
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 3));
  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_CANCELLED, w.Wait(0, &context));
  EXPECT_LT(stopwatch.Elapsed(), test::EpsilonTimeout());
  EXPECT_EQ(3u, context);
  // Don't need to remove waiters from closed dispatchers.

  // Wait for non-zero, finite time for writable and then the dispatcher is
  // closed.
  d = new MockSimpleDispatcher();
  w.Init();
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 4));
  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_CANCELLED,
            w.Wait(2 * test::EpsilonTimeout().InMicroseconds(), &context));
  EXPECT_LT(stopwatch.Elapsed(), test::EpsilonTimeout());
  EXPECT_EQ(4u, context);
  // Don't need to remove waiters from closed dispatchers.
}

TEST(SimpleDispatcherTest, BasicThreaded) {
  test::Stopwatch stopwatch;
  bool did_wait;
  MojoResult result;
  uint32_t context;

  // Wait for readable (already readable).
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    {
      d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
      test::WaiterThread thread(d,
                                MOJO_HANDLE_SIGNAL_READABLE,
                                MOJO_DEADLINE_INDEFINITE,
                                1,
                                &did_wait, &result, &context);
      stopwatch.Start();
      thread.Start();
    }  // Joins the thread.
    // If we closed earlier, then probably we'd get a |MOJO_RESULT_CANCELLED|.
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }
  EXPECT_LT(stopwatch.Elapsed(), test::EpsilonTimeout());
  EXPECT_FALSE(did_wait);
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS, result);

  // Wait for readable and becomes readable after some time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    test::WaiterThread thread(d,
                              MOJO_HANDLE_SIGNAL_READABLE,
                              MOJO_DEADLINE_INDEFINITE,
                              2,
                              &did_wait, &result, &context);
    stopwatch.Start();
    thread.Start();
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the thread.
  base::TimeDelta elapsed = stopwatch.Elapsed();
  EXPECT_GT(elapsed, (2-1) * test::EpsilonTimeout());
  EXPECT_LT(elapsed, (2+1) * test::EpsilonTimeout());
  EXPECT_TRUE(did_wait);
  EXPECT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(2u, context);

  // Wait for readable and becomes never-readable after some time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    test::WaiterThread thread(d,
                              MOJO_HANDLE_SIGNAL_READABLE,
                              MOJO_DEADLINE_INDEFINITE,
                              3,
                              &did_wait, &result, &context);
    stopwatch.Start();
    thread.Start();
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    d->SetSatisfiableSignals(MOJO_HANDLE_SIGNAL_NONE);
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the thread.
  elapsed = stopwatch.Elapsed();
  EXPECT_GT(elapsed, (2-1) * test::EpsilonTimeout());
  EXPECT_LT(elapsed, (2+1) * test::EpsilonTimeout());
  EXPECT_TRUE(did_wait);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
  EXPECT_EQ(3u, context);

  // Wait for readable and dispatcher gets closed.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    test::WaiterThread thread(d,
                              MOJO_HANDLE_SIGNAL_READABLE,
                              MOJO_DEADLINE_INDEFINITE,
                              4,
                              &did_wait, &result, &context);
    stopwatch.Start();
    thread.Start();
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the thread.
  elapsed = stopwatch.Elapsed();
  EXPECT_GT(elapsed, (2-1) * test::EpsilonTimeout());
  EXPECT_LT(elapsed, (2+1) * test::EpsilonTimeout());
  EXPECT_TRUE(did_wait);
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
  EXPECT_EQ(4u, context);

  // Wait for readable and times out.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    {
      test::WaiterThread thread(d,
                                MOJO_HANDLE_SIGNAL_READABLE,
                                2 * test::EpsilonTimeout().InMicroseconds(),
                                5,
                                &did_wait, &result, &context);
      stopwatch.Start();
      thread.Start();
      base::PlatformThread::Sleep(1 * test::EpsilonTimeout());
      // Not what we're waiting for.
      d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_WRITABLE);
    }  // Joins the thread (after its wait times out).
    // If we closed earlier, then probably we'd get a |MOJO_RESULT_CANCELLED|.
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }
  elapsed = stopwatch.Elapsed();
  EXPECT_GT(elapsed, (2-1) * test::EpsilonTimeout());
  EXPECT_LT(elapsed, (2+1) * test::EpsilonTimeout());
  EXPECT_TRUE(did_wait);
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, result);
}

TEST(SimpleDispatcherTest, MultipleWaiters) {
  static const uint32_t kNumWaiters = 20;

  bool did_wait[kNumWaiters];
  MojoResult result[kNumWaiters];
  uint32_t context[kNumWaiters];

  // All wait for readable and becomes readable after some time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    ScopedVector<test::WaiterThread> threads;
    for (uint32_t i = 0; i < kNumWaiters; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_HANDLE_SIGNAL_READABLE,
                                               MOJO_DEADLINE_INDEFINITE,
                                               i,
                                               &did_wait[i],
                                               &result[i],
                                               &context[i]));
      threads.back()->Start();
    }
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the threads.
  for (uint32_t i = 0; i < kNumWaiters; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(MOJO_RESULT_OK, result[i]);
    EXPECT_EQ(i, context[i]);
  }

  // Some wait for readable, some for writable, and becomes readable after some
  // time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    ScopedVector<test::WaiterThread> threads;
    for (uint32_t i = 0; i < kNumWaiters / 2; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_HANDLE_SIGNAL_READABLE,
                                               MOJO_DEADLINE_INDEFINITE,
                                               i,
                                               &did_wait[i],
                                               &result[i],
                                               &context[i]));
      threads.back()->Start();
    }
    for (uint32_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_HANDLE_SIGNAL_WRITABLE,
                                               MOJO_DEADLINE_INDEFINITE,
                                               i,
                                               &did_wait[i],
                                               &result[i],
                                               &context[i]));
      threads.back()->Start();
    }
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
    // This will wake up the ones waiting to write.
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the threads.
  for (uint32_t i = 0; i < kNumWaiters / 2; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(MOJO_RESULT_OK, result[i]);
    EXPECT_EQ(i, context[i]);
  }
  for (uint32_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(MOJO_RESULT_CANCELLED, result[i]);
    EXPECT_EQ(i, context[i]);
  }

  // Some wait for readable, some for writable, and becomes readable and
  // never-writable after some time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    ScopedVector<test::WaiterThread> threads;
    for (uint32_t i = 0; i < kNumWaiters / 2; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_HANDLE_SIGNAL_READABLE,
                                               MOJO_DEADLINE_INDEFINITE,
                                               i,
                                               &did_wait[i],
                                               &result[i],
                                               &context[i]));
      threads.back()->Start();
    }
    for (uint32_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_HANDLE_SIGNAL_WRITABLE,
                                               MOJO_DEADLINE_INDEFINITE,
                                               i,
                                               &did_wait[i],
                                               &result[i],
                                               &context[i]));
      threads.back()->Start();
    }
    base::PlatformThread::Sleep(1 * test::EpsilonTimeout());
    d->SetSatisfiableSignals(MOJO_HANDLE_SIGNAL_READABLE);
    base::PlatformThread::Sleep(1 * test::EpsilonTimeout());
    d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the threads.
  for (uint32_t i = 0; i < kNumWaiters / 2; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(MOJO_RESULT_OK, result[i]);
    EXPECT_EQ(i, context[i]);
  }
  for (uint32_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result[i]);
    EXPECT_EQ(i, context[i]);
  }

  // Some wait for readable, some for writable, and becomes readable after some
  // time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    ScopedVector<test::WaiterThread> threads;
    for (uint32_t i = 0; i < kNumWaiters / 2; i++) {
      threads.push_back(
          new test::WaiterThread(d,
                                 MOJO_HANDLE_SIGNAL_READABLE,
                                 3 * test::EpsilonTimeout().InMicroseconds(),
                                 i,
                                 &did_wait[i], &result[i], &context[i]));
      threads.back()->Start();
    }
    for (uint32_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
      threads.push_back(
          new test::WaiterThread(d,
                                 MOJO_HANDLE_SIGNAL_WRITABLE,
                                 1 * test::EpsilonTimeout().InMicroseconds(),
                                 i,
                                 &did_wait[i], &result[i], &context[i]));
      threads.back()->Start();
    }
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    d->SetSatisfiedSignals(MOJO_HANDLE_SIGNAL_READABLE);
    // All those waiting for writable should have timed out.
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the threads.
  for (uint32_t i = 0; i < kNumWaiters / 2; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(MOJO_RESULT_OK, result[i]);
    EXPECT_EQ(i, context[i]);
  }
  for (uint32_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, result[i]);
  }
}

// TODO(vtl): Stress test?

}  // namespace
}  // namespace system
}  // namespace mojo
