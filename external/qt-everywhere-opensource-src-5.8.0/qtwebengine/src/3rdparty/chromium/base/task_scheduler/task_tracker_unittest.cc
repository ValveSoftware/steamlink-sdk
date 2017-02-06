// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/task_tracker.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/task_scheduler/test_utils.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

// Calls TaskTracker::Shutdown() asynchronously.
class ThreadCallingShutdown : public SimpleThread {
 public:
  explicit ThreadCallingShutdown(TaskTracker* tracker)
      : SimpleThread("ThreadCallingShutdown"),
        tracker_(tracker),
        has_returned_(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED) {}

  // Returns true once the async call to Shutdown() has returned.
  bool has_returned() { return has_returned_.IsSignaled(); }

 private:
  void Run() override {
    tracker_->Shutdown();
    has_returned_.Signal();
  }

  TaskTracker* const tracker_;
  WaitableEvent has_returned_;

  DISALLOW_COPY_AND_ASSIGN(ThreadCallingShutdown);
};

// Runs a task asynchronously.
class ThreadRunningTask : public SimpleThread {
 public:
  ThreadRunningTask(TaskTracker* tracker, const Task* task)
      : SimpleThread("ThreadRunningTask"), tracker_(tracker), task_(task) {}

 private:
  void Run() override { tracker_->RunTask(task_); }

  TaskTracker* const tracker_;
  const Task* const task_;

  DISALLOW_COPY_AND_ASSIGN(ThreadRunningTask);
};

class ScopedSetSingletonAllowed {
 public:
  ScopedSetSingletonAllowed(bool singleton_allowed)
      : previous_value_(
            ThreadRestrictions::SetSingletonAllowed(singleton_allowed)) {}
  ~ScopedSetSingletonAllowed() {
    ThreadRestrictions::SetSingletonAllowed(previous_value_);
  }

 private:
  const bool previous_value_;
};

class TaskSchedulerTaskTrackerTest
    : public testing::TestWithParam<TaskShutdownBehavior> {
 protected:
  TaskSchedulerTaskTrackerTest() = default;

  // Creates a task with |shutdown_behavior|.
  std::unique_ptr<Task> CreateTask(TaskShutdownBehavior shutdown_behavior) {
    return WrapUnique(new Task(
        FROM_HERE,
        Bind(&TaskSchedulerTaskTrackerTest::RunTaskCallback, Unretained(this)),
        TaskTraits().WithShutdownBehavior(shutdown_behavior), TimeDelta()));
  }

  // Calls tracker_->Shutdown() on a new thread. When this returns, Shutdown()
  // method has been entered on the new thread, but it hasn't necessarily
  // returned.
  void CallShutdownAsync() {
    ASSERT_FALSE(thread_calling_shutdown_);
    thread_calling_shutdown_.reset(new ThreadCallingShutdown(&tracker_));
    thread_calling_shutdown_->Start();
    while (!tracker_.IsShuttingDownForTesting() &&
           !tracker_.shutdown_completed()) {
      PlatformThread::YieldCurrentThread();
    }
  }

  void WaitForAsyncShutdownCompleted() {
    ASSERT_TRUE(thread_calling_shutdown_);
    thread_calling_shutdown_->Join();
    EXPECT_TRUE(thread_calling_shutdown_->has_returned());
    EXPECT_TRUE(tracker_.shutdown_completed());
  }

  void VerifyAsyncShutdownInProgress() {
    ASSERT_TRUE(thread_calling_shutdown_);
    EXPECT_FALSE(thread_calling_shutdown_->has_returned());
    EXPECT_FALSE(tracker_.shutdown_completed());
    EXPECT_TRUE(tracker_.IsShuttingDownForTesting());
  }

  TaskTracker tracker_;
  size_t num_tasks_executed_ = 0;

 private:
  void RunTaskCallback() { ++num_tasks_executed_; }

  std::unique_ptr<ThreadCallingShutdown> thread_calling_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerTaskTrackerTest);
};

#define WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED() \
  do {                                      \
    SCOPED_TRACE("");                       \
    WaitForAsyncShutdownCompleted();        \
  } while (false)

#define VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS() \
  do {                                      \
    SCOPED_TRACE("");                       \
    VerifyAsyncShutdownInProgress();        \
  } while (false)

}  // namespace

TEST_P(TaskSchedulerTaskTrackerTest, WillPostAndRunBeforeShutdown) {
  std::unique_ptr<Task> task(CreateTask(GetParam()));

  // Inform |task_tracker_| that |task| will be posted.
  EXPECT_TRUE(tracker_.WillPostTask(task.get()));

  // Run the task.
  EXPECT_EQ(0U, num_tasks_executed_);
  tracker_.RunTask(task.get());
  EXPECT_EQ(1U, num_tasks_executed_);

  // Shutdown() shouldn't block.
  tracker_.Shutdown();
}

TEST_P(TaskSchedulerTaskTrackerTest, WillPostAndRunLongTaskBeforeShutdown) {
  // Create a task that will block until |event| is signaled.
  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<Task> blocked_task(
      new Task(FROM_HERE, Bind(&WaitableEvent::Wait, Unretained(&event)),
               TaskTraits().WithShutdownBehavior(GetParam()), TimeDelta()));

  // Inform |task_tracker_| that |blocked_task| will be posted.
  EXPECT_TRUE(tracker_.WillPostTask(blocked_task.get()));

  // Run the task asynchronouly.
  ThreadRunningTask thread_running_task(&tracker_, blocked_task.get());
  thread_running_task.Start();

  // Initiate shutdown while the task is running.
  CallShutdownAsync();

  if (GetParam() == TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN) {
    // Shutdown should complete even with a CONTINUE_ON_SHUTDOWN in progress.
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
  } else {
    // Shutdown should block with any non CONTINUE_ON_SHUTDOWN task in progress.
    VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();
  }

  // Unblock the task.
  event.Signal();
  thread_running_task.Join();

  // Shutdown should now complete for a non CONTINUE_ON_SHUTDOWN task.
  if (GetParam() != TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(TaskSchedulerTaskTrackerTest, WillPostBeforeShutdownRunDuringShutdown) {
  // Inform |task_tracker_| that a task will be posted.
  std::unique_ptr<Task> task(CreateTask(GetParam()));
  EXPECT_TRUE(tracker_.WillPostTask(task.get()));

  // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted just to
  // block shutdown.
  std::unique_ptr<Task> block_shutdown_task(
      CreateTask(TaskShutdownBehavior::BLOCK_SHUTDOWN));
  EXPECT_TRUE(tracker_.WillPostTask(block_shutdown_task.get()));

  // Call Shutdown() asynchronously.
  CallShutdownAsync();
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

  // Try to run |task|. It should only run it it's BLOCK_SHUTDOWN. Otherwise it
  // should be discarded.
  EXPECT_EQ(0U, num_tasks_executed_);
  tracker_.RunTask(task.get());
  EXPECT_EQ(GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN ? 1U : 0U,
            num_tasks_executed_);
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

  // Unblock shutdown by running the remaining BLOCK_SHUTDOWN task.
  tracker_.RunTask(block_shutdown_task.get());
  EXPECT_EQ(GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN ? 2U : 1U,
            num_tasks_executed_);
  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(TaskSchedulerTaskTrackerTest, WillPostBeforeShutdownRunAfterShutdown) {
  // Inform |task_tracker_| that a task will be posted.
  std::unique_ptr<Task> task(CreateTask(GetParam()));
  EXPECT_TRUE(tracker_.WillPostTask(task.get()));

  // Call Shutdown() asynchronously.
  CallShutdownAsync();
  EXPECT_EQ(0U, num_tasks_executed_);

  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

    // Run the task to unblock shutdown.
    tracker_.RunTask(task.get());
    EXPECT_EQ(1U, num_tasks_executed_);
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();

    // It is not possible to test running a BLOCK_SHUTDOWN task posted before
    // shutdown after shutdown because Shutdown() won't return if there are
    // pending BLOCK_SHUTDOWN tasks.
  } else {
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();

    // The task shouldn't be allowed to run after shutdown.
    tracker_.RunTask(task.get());
    EXPECT_EQ(0U, num_tasks_executed_);
  }
}

TEST_P(TaskSchedulerTaskTrackerTest, WillPostAndRunDuringShutdown) {
  // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted just to
  // block shutdown.
  std::unique_ptr<Task> block_shutdown_task(
      CreateTask(TaskShutdownBehavior::BLOCK_SHUTDOWN));
  EXPECT_TRUE(tracker_.WillPostTask(block_shutdown_task.get()));

  // Call Shutdown() asynchronously.
  CallShutdownAsync();
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted.
    std::unique_ptr<Task> task(CreateTask(GetParam()));
    EXPECT_TRUE(tracker_.WillPostTask(task.get()));

    // Run the BLOCK_SHUTDOWN task.
    EXPECT_EQ(0U, num_tasks_executed_);
    tracker_.RunTask(task.get());
    EXPECT_EQ(1U, num_tasks_executed_);
  } else {
    // It shouldn't be allowed to post a non BLOCK_SHUTDOWN task.
    std::unique_ptr<Task> task(CreateTask(GetParam()));
    EXPECT_FALSE(tracker_.WillPostTask(task.get()));

    // Don't try to run the task, because it wasn't allowed to be posted.
  }

  // Unblock shutdown by running |block_shutdown_task|.
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();
  tracker_.RunTask(block_shutdown_task.get());
  EXPECT_EQ(GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN ? 2U : 1U,
            num_tasks_executed_);
  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(TaskSchedulerTaskTrackerTest, WillPostAfterShutdown) {
  tracker_.Shutdown();

  std::unique_ptr<Task> task(CreateTask(GetParam()));

  // |task_tracker_| shouldn't allow a task to be posted after shutdown.
  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    EXPECT_DCHECK_DEATH({ tracker_.WillPostTask(task.get()); }, "");
  } else {
    EXPECT_FALSE(tracker_.WillPostTask(task.get()));
  }
}

// Verify that BLOCK_SHUTDOWN and SKIP_ON_SHUTDOWN tasks can
// AssertSingletonAllowed() but CONTINUE_ON_SHUTDOWN tasks can't.
TEST_P(TaskSchedulerTaskTrackerTest, SingletonAllowed) {
  const bool can_use_singletons =
      (GetParam() != TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN);

  TaskTracker tracker;
  Task task(FROM_HERE, Bind(&ThreadRestrictions::AssertSingletonAllowed),
            TaskTraits().WithShutdownBehavior(GetParam()), TimeDelta());
  EXPECT_TRUE(tracker.WillPostTask(&task));

  // Set the singleton allowed bit to the opposite of what it is expected to be
  // when |tracker| runs |task| to verify that |tracker| actually sets the
  // correct value.
  ScopedSetSingletonAllowed scoped_singleton_allowed(!can_use_singletons);

  // Running the task should fail iff the task isn't allowed to use singletons.
  if (can_use_singletons) {
    tracker.RunTask(&task);
  } else {
    EXPECT_DCHECK_DEATH({ tracker.RunTask(&task); }, "");
  }
}

static void RunTaskRunnerHandleVerificationTask(TaskTracker* tracker,
                                         const Task* verify_task) {
  // Pretend |verify_task| is posted to respect TaskTracker's contract.
  EXPECT_TRUE(tracker->WillPostTask(verify_task));

  // Confirm that the test conditions are right (no TaskRunnerHandles set
  // already).
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());

  tracker->RunTask(verify_task);

  // TaskRunnerHandle state is reset outside of task's scope.
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());
}

static void VerifyNoTaskRunnerHandle() {
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());
}

TEST_P(TaskSchedulerTaskTrackerTest, TaskRunnerHandleIsNotSetOnParallel) {
  // Create a task that will verify that TaskRunnerHandles are not set in its
  // scope per no TaskRunner ref being set to it.
  std::unique_ptr<Task> verify_task(
      new Task(FROM_HERE, Bind(&VerifyNoTaskRunnerHandle),
               TaskTraits().WithShutdownBehavior(GetParam()), TimeDelta()));

  RunTaskRunnerHandleVerificationTask(&tracker_, verify_task.get());
}

static void VerifySequencedTaskRunnerHandle(
    const SequencedTaskRunner* expected_task_runner) {
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_TRUE(SequencedTaskRunnerHandle::IsSet());
  EXPECT_EQ(expected_task_runner, SequencedTaskRunnerHandle::Get());
}

TEST_P(TaskSchedulerTaskTrackerTest,
       SequencedTaskRunnerHandleIsSetOnSequenced) {
  scoped_refptr<SequencedTaskRunner> test_task_runner(new TestSimpleTaskRunner);

  // Create a task that will verify that SequencedTaskRunnerHandle is properly
  // set to |test_task_runner| in its scope per |sequenced_task_runner_ref|
  // being set to it.
  std::unique_ptr<Task> verify_task(
      new Task(FROM_HERE, Bind(&VerifySequencedTaskRunnerHandle,
                               base::Unretained(test_task_runner.get())),
               TaskTraits().WithShutdownBehavior(GetParam()), TimeDelta()));
  verify_task->sequenced_task_runner_ref = test_task_runner;

  RunTaskRunnerHandleVerificationTask(&tracker_, verify_task.get());
}

static void VerifyThreadTaskRunnerHandle(
    const SingleThreadTaskRunner* expected_task_runner) {
  EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
  // SequencedTaskRunnerHandle inherits ThreadTaskRunnerHandle for thread.
  EXPECT_TRUE(SequencedTaskRunnerHandle::IsSet());
  EXPECT_EQ(expected_task_runner, ThreadTaskRunnerHandle::Get());
}

TEST_P(TaskSchedulerTaskTrackerTest,
       ThreadTaskRunnerHandleIsSetOnSingleThreaded) {
  scoped_refptr<SingleThreadTaskRunner> test_task_runner(
      new TestSimpleTaskRunner);

  // Create a task that will verify that ThreadTaskRunnerHandle is properly set
  // to |test_task_runner| in its scope per |single_thread_task_runner_ref|
  // being set on it.
  std::unique_ptr<Task> verify_task(
      new Task(FROM_HERE, Bind(&VerifyThreadTaskRunnerHandle,
                               base::Unretained(test_task_runner.get())),
               TaskTraits().WithShutdownBehavior(GetParam()), TimeDelta()));
  verify_task->single_thread_task_runner_ref = test_task_runner;

  RunTaskRunnerHandleVerificationTask(&tracker_, verify_task.get());
}

INSTANTIATE_TEST_CASE_P(
    ContinueOnShutdown,
    TaskSchedulerTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN));
INSTANTIATE_TEST_CASE_P(
    SkipOnShutdown,
    TaskSchedulerTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::SKIP_ON_SHUTDOWN));
INSTANTIATE_TEST_CASE_P(
    BlockShutdown,
    TaskSchedulerTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::BLOCK_SHUTDOWN));

}  // namespace internal
}  // namespace base
