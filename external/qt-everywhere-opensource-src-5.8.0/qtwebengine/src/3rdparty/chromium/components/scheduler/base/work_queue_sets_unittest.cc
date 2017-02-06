// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/work_queue_sets.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "components/scheduler/base/work_queue.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace scheduler {
class TimeDomain;

namespace internal {

class WorkQueueSetsTest : public testing::Test {
 public:
  void SetUp() override {
    work_queue_sets_.reset(new WorkQueueSets(kNumSets, "test"));
  }

  void TearDown() override {
    for (std::unique_ptr<WorkQueue>& work_queue : work_queues_) {
      if (work_queue->work_queue_sets())
        work_queue_sets_->RemoveQueue(work_queue.get());
    }
  }

 protected:
  enum {
    kNumSets = 5  // An arbitary choice.
  };

  WorkQueue* NewTaskQueue(const char* queue_name) {
    WorkQueue* queue = new WorkQueue(nullptr, "test");
    work_queues_.push_back(base::WrapUnique(queue));
    work_queue_sets_->AddQueue(queue, TaskQueue::CONTROL_PRIORITY);
    return queue;
  }

  TaskQueueImpl::Task FakeTaskWithEnqueueOrder(int enqueue_order) {
    TaskQueueImpl::Task fake_task(FROM_HERE, base::Closure(), base::TimeTicks(),
                                  0, true);
    fake_task.set_enqueue_order(enqueue_order);
    return fake_task;
  }

  std::vector<std::unique_ptr<WorkQueue>> work_queues_;
  std::unique_ptr<WorkQueueSets> work_queue_sets_;
};

TEST_F(WorkQueueSetsTest, ChangeSetIndex) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  size_t set = TaskQueue::NORMAL_PRIORITY;
  work_queue_sets_->ChangeSetIndex(work_queue, set);

  EXPECT_EQ(set, work_queue->work_queue_set_index());
}

TEST_F(WorkQueueSetsTest, GetOldestQueueInSet_QueueEmpty) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  size_t set = TaskQueue::NORMAL_PRIORITY;
  work_queue_sets_->ChangeSetIndex(work_queue, set);

  WorkQueue* selected_work_queue;
  EXPECT_FALSE(
      work_queue_sets_->GetOldestQueueInSet(set, &selected_work_queue));
}

TEST_F(WorkQueueSetsTest, OnPushQueue) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  size_t set = TaskQueue::NORMAL_PRIORITY;
  work_queue_sets_->ChangeSetIndex(work_queue, set);

  WorkQueue* selected_work_queue;
  EXPECT_FALSE(
      work_queue_sets_->GetOldestQueueInSet(set, &selected_work_queue));

  work_queue->Push(FakeTaskWithEnqueueOrder(10));
  work_queue_sets_->OnPushQueue(work_queue);

  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(set, &selected_work_queue));
  EXPECT_EQ(work_queue, selected_work_queue);
}

TEST_F(WorkQueueSetsTest, GetOldestQueueInSet_SingleTaskInSet) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  work_queue->Push(FakeTaskWithEnqueueOrder(10));
  size_t set = 1;
  work_queue_sets_->ChangeSetIndex(work_queue, set);

  WorkQueue* selected_work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(set, &selected_work_queue));
  EXPECT_EQ(work_queue, selected_work_queue);
}

TEST_F(WorkQueueSetsTest, GetOldestQueueInSet_MultipleAgesInSet) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue2");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 2;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);

  WorkQueue* selected_work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(set, &selected_work_queue));
  EXPECT_EQ(queue3, selected_work_queue);
}

TEST_F(WorkQueueSetsTest, OnPopQueue) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(3));
  queue2->Push(FakeTaskWithEnqueueOrder(1));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 3;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);

  WorkQueue* selected_work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(set, &selected_work_queue));
  EXPECT_EQ(queue2, selected_work_queue);

  queue2->PopTaskForTest();
  work_queue_sets_->OnPopQueue(queue2);

  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(set, &selected_work_queue));
  EXPECT_EQ(queue2, selected_work_queue);
}

TEST_F(WorkQueueSetsTest, OnPopQueue_QueueBecomesEmpty) {
  WorkQueue* queue1 = NewTaskQueue("queue");
  WorkQueue* queue2 = NewTaskQueue("queue");
  WorkQueue* queue3 = NewTaskQueue("queue");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 4;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);

  WorkQueue* selected_work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(set, &selected_work_queue));
  EXPECT_EQ(queue3, selected_work_queue);

  queue3->PopTaskForTest();
  work_queue_sets_->OnPopQueue(queue3);

  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(set, &selected_work_queue));
  EXPECT_EQ(queue2, selected_work_queue);
}

TEST_F(WorkQueueSetsTest,
       GetOldestQueueInSet_MultipleAgesInSetIntegerRollover) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  queue1->Push(FakeTaskWithEnqueueOrder(0x7ffffff1));
  queue2->Push(FakeTaskWithEnqueueOrder(0x7ffffff0));
  queue3->Push(FakeTaskWithEnqueueOrder(-0x7ffffff1));
  size_t set = 1;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);

  WorkQueue* selected_work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(set, &selected_work_queue));
  EXPECT_EQ(queue2, selected_work_queue);
}

TEST_F(WorkQueueSetsTest, GetOldestQueueInSet_MultipleAgesInSet_RemoveQueue) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 1;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);
  work_queue_sets_->RemoveQueue(queue3);

  WorkQueue* selected_work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(set, &selected_work_queue));
  EXPECT_EQ(queue2, selected_work_queue);
}

TEST_F(WorkQueueSetsTest, ChangeSetIndex_Complex) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  WorkQueue* queue4 = NewTaskQueue("queue4");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  queue4->Push(FakeTaskWithEnqueueOrder(3));
  size_t set1 = 1;
  size_t set2 = 2;
  work_queue_sets_->ChangeSetIndex(queue1, set1);
  work_queue_sets_->ChangeSetIndex(queue2, set1);
  work_queue_sets_->ChangeSetIndex(queue3, set2);
  work_queue_sets_->ChangeSetIndex(queue4, set2);

  WorkQueue* selected_work_queue;
  EXPECT_TRUE(
      work_queue_sets_->GetOldestQueueInSet(set1, &selected_work_queue));
  EXPECT_EQ(queue2, selected_work_queue);

  EXPECT_TRUE(
      work_queue_sets_->GetOldestQueueInSet(set2, &selected_work_queue));
  EXPECT_EQ(queue4, selected_work_queue);

  work_queue_sets_->ChangeSetIndex(queue4, set1);

  EXPECT_TRUE(
      work_queue_sets_->GetOldestQueueInSet(set1, &selected_work_queue));
  EXPECT_EQ(queue4, selected_work_queue);

  EXPECT_TRUE(
      work_queue_sets_->GetOldestQueueInSet(set2, &selected_work_queue));
  EXPECT_EQ(queue3, selected_work_queue);
}

TEST_F(WorkQueueSetsTest, IsSetEmpty_NoWork) {
  size_t set = 2;
  EXPECT_TRUE(work_queue_sets_->IsSetEmpty(set));

  WorkQueue* work_queue = NewTaskQueue("queue");
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_TRUE(work_queue_sets_->IsSetEmpty(set));
}

TEST_F(WorkQueueSetsTest, IsSetEmpty_Work) {
  size_t set = 2;
  EXPECT_TRUE(work_queue_sets_->IsSetEmpty(set));

  WorkQueue* work_queue = NewTaskQueue("queue");
  work_queue->Push(FakeTaskWithEnqueueOrder(1));
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_FALSE(work_queue_sets_->IsSetEmpty(set));

  work_queue->PopTaskForTest();
  work_queue_sets_->OnPopQueue(work_queue);
  EXPECT_TRUE(work_queue_sets_->IsSetEmpty(set));
}

}  // namespace internal
}  // namespace scheduler
