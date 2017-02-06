// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_manager.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_manager_delegate_for_test.h"
#include "components/scheduler/base/task_queue_selector.h"
#include "components/scheduler/base/work_queue_sets.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace scheduler {

class TaskQueueManagerPerfTest : public testing::Test {
 public:
  TaskQueueManagerPerfTest()
      : num_queues_(0),
        max_tasks_in_flight_(0),
        num_tasks_in_flight_(0),
        num_tasks_to_post_(0),
        num_tasks_to_run_(0) {}

  void SetUp() override {
    if (base::ThreadTicks::IsSupported())
      base::ThreadTicks::WaitUntilInitialized();
  }

  void Initialize(size_t num_queues) {
    num_queues_ = num_queues;
    message_loop_.reset(new base::MessageLoop());
    manager_ = base::WrapUnique(new TaskQueueManager(
        TaskQueueManagerDelegateForTest::Create(
            message_loop_->task_runner(),
            base::WrapUnique(new base::DefaultTickClock())),
        "fake.category", "fake.category", "fake.category.debug"));
    for (size_t i = 0; i < num_queues; i++)
      queues_.push_back(manager_->NewTaskQueue(TaskQueue::Spec("test")));
  }

  void TestDelayedTask() {
    if (--num_tasks_to_run_ == 0) {
      message_loop_->QuitWhenIdle();
    }

    num_tasks_in_flight_--;
    // NOTE there are only up to max_tasks_in_flight_ pending delayed tasks at
    // any one time.  Thanks to the lower_num_tasks_to_post going to zero if
    // there are a lot of tasks in flight, the total number of task in flight at
    // any one time is very variable.
    unsigned int lower_num_tasks_to_post =
        num_tasks_in_flight_ < (max_tasks_in_flight_ / 2) ? 1 : 0;
    unsigned int max_tasks_to_post =
        num_tasks_to_post_ % 2 ? lower_num_tasks_to_post : 10;
    for (unsigned int i = 0;
         i < max_tasks_to_post && num_tasks_in_flight_ < max_tasks_in_flight_ &&
         num_tasks_to_post_ > 0;
         i++) {
      // Choose a queue weighted towards queue 0.
      unsigned int queue = num_tasks_to_post_ % (num_queues_ + 1);
      if (queue == num_queues_) {
        queue = 0;
      }
      // Simulate a mix of short and longer delays.
      unsigned int delay =
          num_tasks_to_post_ % 2 ? 1 : (10 + num_tasks_to_post_ % 10);
      queues_[queue]->PostDelayedTask(
          FROM_HERE, base::Bind(&TaskQueueManagerPerfTest::TestDelayedTask,
                                base::Unretained(this)),
          base::TimeDelta::FromMicroseconds(delay));
      num_tasks_in_flight_++;
      num_tasks_to_post_--;
    }
  }

  void ResetAndCallTestDelayedTask(unsigned int num_tasks_to_run) {
    num_tasks_in_flight_ = 1;
    num_tasks_to_post_ = num_tasks_to_run;
    num_tasks_to_run_ = num_tasks_to_run;
    TestDelayedTask();
  }

  void Benchmark(const std::string& trace, const base::Closure& test_task) {
    base::ThreadTicks start = base::ThreadTicks::Now();
    base::ThreadTicks now;
    unsigned long long num_iterations = 0;
    do {
      test_task.Run();
      message_loop_->Run();
      now = base::ThreadTicks::Now();
      num_iterations++;
    } while (now - start < base::TimeDelta::FromSeconds(5));
    perf_test::PrintResult(
        "task", "", trace,
        (now - start).InMicroseconds() / static_cast<double>(num_iterations),
        "us/run", true);
  }

  size_t num_queues_;
  unsigned int max_tasks_in_flight_;
  unsigned int num_tasks_in_flight_;
  unsigned int num_tasks_to_post_;
  unsigned int num_tasks_to_run_;
  std::unique_ptr<TaskQueueManager> manager_;
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::vector<scoped_refptr<base::SingleThreadTaskRunner>> queues_;
};

TEST_F(TaskQueueManagerPerfTest, RunTenThousandDelayedTasks_OneQueue) {
  if (!base::ThreadTicks::IsSupported())
    return;
  Initialize(1u);

  max_tasks_in_flight_ = 200;
  Benchmark("run 10000 delayed tasks with one queue",
            base::Bind(&TaskQueueManagerPerfTest::ResetAndCallTestDelayedTask,
                       base::Unretained(this), 10000));
}

TEST_F(TaskQueueManagerPerfTest, RunTenThousandDelayedTasks_FourQueues) {
  if (!base::ThreadTicks::IsSupported())
    return;
  Initialize(4u);

  max_tasks_in_flight_ = 200;
  Benchmark("run 10000 delayed tasks with four queues",
            base::Bind(&TaskQueueManagerPerfTest::ResetAndCallTestDelayedTask,
                       base::Unretained(this), 10000));
}

TEST_F(TaskQueueManagerPerfTest, RunTenThousandDelayedTasks_EightQueues) {
  if (!base::ThreadTicks::IsSupported())
    return;
  Initialize(8u);

  max_tasks_in_flight_ = 200;
  Benchmark("run 10000 delayed tasks with eight queues",
            base::Bind(&TaskQueueManagerPerfTest::ResetAndCallTestDelayedTask,
                       base::Unretained(this), 10000));
}

TEST_F(TaskQueueManagerPerfTest, RunTenThousandDelayedTasks_ThirtyTwoQueues) {
  if (!base::ThreadTicks::IsSupported())
    return;
  Initialize(32u);

  max_tasks_in_flight_ = 200;
  Benchmark("run 10000 delayed tasks with eight queues",
            base::Bind(&TaskQueueManagerPerfTest::ResetAndCallTestDelayedTask,
                       base::Unretained(this), 10000));
}

// TODO(alexclarke): Add additional tests with different mixes of non-delayed vs
// delayed tasks.

}  // namespace scheduler
