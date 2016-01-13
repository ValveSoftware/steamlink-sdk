// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE(vtl): Some of these tests are inherently flaky (e.g., if run on a
// heavily-loaded system). Sorry. |test::EpsilonTimeout()| may be increased to
// increase tolerance and reduce observed flakiness (though doing so reduces the
// meaningfulness of the test).

#include "mojo/system/waiter_list.h"

#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "base/time/time.h"
#include "mojo/system/handle_signals_state.h"
#include "mojo/system/test_utils.h"
#include "mojo/system/waiter.h"
#include "mojo/system/waiter_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

TEST(WaiterListTest, BasicCancel) {
  MojoResult result;
  uint32_t context;

  // Cancel immediately after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result, &context);
    waiter_list.AddWaiter(thread.waiter(), MOJO_HANDLE_SIGNAL_READABLE, 1);
    thread.Start();
    waiter_list.CancelAllWaiters();
    waiter_list.RemoveWaiter(thread.waiter());  // Double-remove okay.
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
  EXPECT_EQ(1u, context);

  // Cancel before after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result, &context);
    waiter_list.AddWaiter(thread.waiter(), MOJO_HANDLE_SIGNAL_WRITABLE, 2);
    waiter_list.CancelAllWaiters();
    thread.Start();
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
  EXPECT_EQ(2u, context);

  // Cancel some time after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result, &context);
    waiter_list.AddWaiter(thread.waiter(), MOJO_HANDLE_SIGNAL_READABLE, 3);
    thread.Start();
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    waiter_list.CancelAllWaiters();
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
  EXPECT_EQ(3u, context);
}

TEST(WaiterListTest, BasicAwakeSatisfied) {
  MojoResult result;
  uint32_t context;

  // Awake immediately after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result, &context);
    waiter_list.AddWaiter(thread.waiter(), MOJO_HANDLE_SIGNAL_READABLE, 1);
    thread.Start();
    waiter_list.AwakeWaitersForStateChange(
        HandleSignalsState(MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_HANDLE_SIGNAL_READABLE |
                               MOJO_HANDLE_SIGNAL_WRITABLE));
    waiter_list.RemoveWaiter(thread.waiter());
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(1u, context);

  // Awake before after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result, &context);
    waiter_list.AddWaiter(thread.waiter(), MOJO_HANDLE_SIGNAL_WRITABLE, 2);
    waiter_list.AwakeWaitersForStateChange(
        HandleSignalsState(MOJO_HANDLE_SIGNAL_WRITABLE,
                           MOJO_HANDLE_SIGNAL_READABLE |
                               MOJO_HANDLE_SIGNAL_WRITABLE));
    waiter_list.RemoveWaiter(thread.waiter());
    waiter_list.RemoveWaiter(thread.waiter());  // Double-remove okay.
    thread.Start();
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(2u, context);

  // Awake some time after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result, &context);
    waiter_list.AddWaiter(thread.waiter(), MOJO_HANDLE_SIGNAL_READABLE, 3);
    thread.Start();
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    waiter_list.AwakeWaitersForStateChange(
        HandleSignalsState(MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_HANDLE_SIGNAL_READABLE |
                               MOJO_HANDLE_SIGNAL_WRITABLE));
    waiter_list.RemoveWaiter(thread.waiter());
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(3u, context);
}

TEST(WaiterListTest, BasicAwakeUnsatisfiable) {
  MojoResult result;
  uint32_t context;

  // Awake (for unsatisfiability) immediately after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result, &context);
    waiter_list.AddWaiter(thread.waiter(), MOJO_HANDLE_SIGNAL_READABLE, 1);
    thread.Start();
    waiter_list.AwakeWaitersForStateChange(
        HandleSignalsState(MOJO_HANDLE_SIGNAL_NONE,
                           MOJO_HANDLE_SIGNAL_WRITABLE));
    waiter_list.RemoveWaiter(thread.waiter());
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
  EXPECT_EQ(1u, context);

  // Awake (for unsatisfiability) before after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result, &context);
    waiter_list.AddWaiter(thread.waiter(), MOJO_HANDLE_SIGNAL_WRITABLE, 2);
    waiter_list.AwakeWaitersForStateChange(
        HandleSignalsState(MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_HANDLE_SIGNAL_READABLE));
    waiter_list.RemoveWaiter(thread.waiter());
    thread.Start();
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
  EXPECT_EQ(2u, context);

  // Awake (for unsatisfiability) some time after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result, &context);
    waiter_list.AddWaiter(thread.waiter(), MOJO_HANDLE_SIGNAL_READABLE, 3);
    thread.Start();
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    waiter_list.AwakeWaitersForStateChange(
        HandleSignalsState(MOJO_HANDLE_SIGNAL_NONE,
                           MOJO_HANDLE_SIGNAL_WRITABLE));
    waiter_list.RemoveWaiter(thread.waiter());
    waiter_list.RemoveWaiter(thread.waiter());  // Double-remove okay.
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
  EXPECT_EQ(3u, context);
}

TEST(WaiterListTest, MultipleWaiters) {
  MojoResult result1;
  MojoResult result2;
  MojoResult result3;
  MojoResult result4;
  uint32_t context1;
  uint32_t context2;
  uint32_t context3;
  uint32_t context4;

  // Cancel two waiters.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread1(&result1, &context1);
    waiter_list.AddWaiter(thread1.waiter(), MOJO_HANDLE_SIGNAL_READABLE, 1);
    thread1.Start();
    test::SimpleWaiterThread thread2(&result2, &context2);
    waiter_list.AddWaiter(thread2.waiter(), MOJO_HANDLE_SIGNAL_WRITABLE, 2);
    thread2.Start();
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    waiter_list.CancelAllWaiters();
  }  // Join threads.
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result1);
  EXPECT_EQ(1u, context1);
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result2);
  EXPECT_EQ(2u, context2);

  // Awake one waiter, cancel other.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread1(&result1, &context1);
    waiter_list.AddWaiter(thread1.waiter(), MOJO_HANDLE_SIGNAL_READABLE, 3);
    thread1.Start();
    test::SimpleWaiterThread thread2(&result2, &context2);
    waiter_list.AddWaiter(thread2.waiter(), MOJO_HANDLE_SIGNAL_WRITABLE, 4);
    thread2.Start();
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    waiter_list.AwakeWaitersForStateChange(
        HandleSignalsState(MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_HANDLE_SIGNAL_READABLE |
                               MOJO_HANDLE_SIGNAL_WRITABLE));
    waiter_list.RemoveWaiter(thread1.waiter());
    waiter_list.CancelAllWaiters();
  }  // Join threads.
  EXPECT_EQ(MOJO_RESULT_OK, result1);
  EXPECT_EQ(3u, context1);
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result2);
  EXPECT_EQ(4u, context2);

  // Cancel one waiter, awake other for unsatisfiability.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread1(&result1, &context1);
    waiter_list.AddWaiter(thread1.waiter(), MOJO_HANDLE_SIGNAL_READABLE, 5);
    thread1.Start();
    test::SimpleWaiterThread thread2(&result2, &context2);
    waiter_list.AddWaiter(thread2.waiter(), MOJO_HANDLE_SIGNAL_WRITABLE, 6);
    thread2.Start();
    base::PlatformThread::Sleep(2 * test::EpsilonTimeout());
    waiter_list.AwakeWaitersForStateChange(
        HandleSignalsState(MOJO_HANDLE_SIGNAL_NONE,
                           MOJO_HANDLE_SIGNAL_READABLE));
    waiter_list.RemoveWaiter(thread2.waiter());
    waiter_list.CancelAllWaiters();
  }  // Join threads.
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result1);
  EXPECT_EQ(5u, context1);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result2);
  EXPECT_EQ(6u, context2);

  // Cancel one waiter, awake other for unsatisfiability.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread1(&result1, &context1);
    waiter_list.AddWaiter(thread1.waiter(), MOJO_HANDLE_SIGNAL_READABLE, 7);
    thread1.Start();

    base::PlatformThread::Sleep(1 * test::EpsilonTimeout());

    // Should do nothing.
    waiter_list.AwakeWaitersForStateChange(
        HandleSignalsState(MOJO_HANDLE_SIGNAL_NONE,
                           MOJO_HANDLE_SIGNAL_READABLE |
                               MOJO_HANDLE_SIGNAL_WRITABLE));

    test::SimpleWaiterThread thread2(&result2, &context2);
    waiter_list.AddWaiter(thread2.waiter(), MOJO_HANDLE_SIGNAL_WRITABLE, 8);
    thread2.Start();

    base::PlatformThread::Sleep(1 * test::EpsilonTimeout());

    // Awake #1.
    waiter_list.AwakeWaitersForStateChange(
        HandleSignalsState(MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_HANDLE_SIGNAL_READABLE |
                               MOJO_HANDLE_SIGNAL_WRITABLE));
    waiter_list.RemoveWaiter(thread1.waiter());

    base::PlatformThread::Sleep(1 * test::EpsilonTimeout());

    test::SimpleWaiterThread thread3(&result3, &context3);
    waiter_list.AddWaiter(thread3.waiter(), MOJO_HANDLE_SIGNAL_WRITABLE, 9);
    thread3.Start();

    test::SimpleWaiterThread thread4(&result4, &context4);
    waiter_list.AddWaiter(thread4.waiter(), MOJO_HANDLE_SIGNAL_READABLE, 10);
    thread4.Start();

    base::PlatformThread::Sleep(1 * test::EpsilonTimeout());

    // Awake #2 and #3 for unsatisfiability.
    waiter_list.AwakeWaitersForStateChange(
        HandleSignalsState(MOJO_HANDLE_SIGNAL_NONE,
                           MOJO_HANDLE_SIGNAL_READABLE));
    waiter_list.RemoveWaiter(thread2.waiter());
    waiter_list.RemoveWaiter(thread3.waiter());

    // Cancel #4.
    waiter_list.CancelAllWaiters();
  }  // Join threads.
  EXPECT_EQ(MOJO_RESULT_OK, result1);
  EXPECT_EQ(7u, context1);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result2);
  EXPECT_EQ(8u, context2);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result3);
  EXPECT_EQ(9u, context3);
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result4);
  EXPECT_EQ(10u, context4);
}

}  // namespace
}  // namespace system
}  // namespace mojo
