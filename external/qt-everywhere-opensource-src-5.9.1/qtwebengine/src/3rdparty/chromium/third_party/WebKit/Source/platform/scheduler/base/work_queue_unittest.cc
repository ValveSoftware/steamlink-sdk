// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/work_queue.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/work_queue_sets.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {
namespace scheduler {
namespace internal {
namespace {
void NopTask() {}
}

class WorkQueueTest : public testing::Test {
 public:
  void SetUp() override {
    time_domain_.reset(new RealTimeDomain(""));
    task_queue_ = make_scoped_refptr(
        new TaskQueueImpl(nullptr, time_domain_.get(),
                          TaskQueue::Spec(TaskQueue::QueueType::TEST), "", ""));

    work_queue_.reset(new WorkQueue(task_queue_.get(), "test"));
    work_queue_sets_.reset(new WorkQueueSets(1, "test"));
    work_queue_sets_->AddQueue(work_queue_.get(), 0);

    incoming_queue_.reset(new std::queue<TaskQueueImpl::Task>());
  }

  void TearDown() override { work_queue_sets_->RemoveQueue(work_queue_.get()); }

 protected:
  TaskQueueImpl::Task FakeTaskWithEnqueueOrder(int enqueue_order) {
    TaskQueueImpl::Task fake_task(FROM_HERE, base::Bind(&NopTask),
                                  base::TimeTicks(), 0, true);
    fake_task.set_enqueue_order(enqueue_order);
    return fake_task;
  }

  std::unique_ptr<RealTimeDomain> time_domain_;
  scoped_refptr<TaskQueueImpl> task_queue_;
  std::unique_ptr<WorkQueue> work_queue_;
  std::unique_ptr<WorkQueueSets> work_queue_sets_;
  std::unique_ptr<std::queue<TaskQueueImpl::Task>> incoming_queue_;
};

TEST_F(WorkQueueTest, Empty) {
  EXPECT_TRUE(work_queue_->Empty());
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->Empty());
}

TEST_F(WorkQueueTest, Empty_IgnoresFences) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  work_queue_->InsertFence(1);
  EXPECT_FALSE(work_queue_->Empty());
}

TEST_F(WorkQueueTest, GetFrontTaskEnqueueOrderQueueEmpty) {
  EnqueueOrder enqueue_order;
  EXPECT_FALSE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
}

TEST_F(WorkQueueTest, GetFrontTaskEnqueueOrder) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  EnqueueOrder enqueue_order;
  EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_EQ(2ull, enqueue_order);
}

TEST_F(WorkQueueTest, GetFrontTaskQueueEmpty) {
  EXPECT_EQ(nullptr, work_queue_->GetFrontTask());
}

TEST_F(WorkQueueTest, GetFrontTask) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  ASSERT_NE(nullptr, work_queue_->GetFrontTask());
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());
}

TEST_F(WorkQueueTest, GetBackTask_Empty) {
  EXPECT_EQ(nullptr, work_queue_->GetBackTask());
}

TEST_F(WorkQueueTest, GetBackTask) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  ASSERT_NE(nullptr, work_queue_->GetBackTask());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, Push) {
  WorkQueue* work_queue;
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_EQ(work_queue_.get(), work_queue);
}

TEST_F(WorkQueueTest, PushAfterFenceHit) {
  work_queue_->InsertFence(1);
  WorkQueue* work_queue;
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
}

TEST_F(WorkQueueTest, SwapLocked) {
  incoming_queue_->push(FakeTaskWithEnqueueOrder(2));
  incoming_queue_->push(FakeTaskWithEnqueueOrder(3));
  incoming_queue_->push(FakeTaskWithEnqueueOrder(4));

  WorkQueue* work_queue;
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_TRUE(work_queue_->Empty());
  work_queue_->SwapLocked(*incoming_queue_.get());

  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_TRUE(incoming_queue_->empty());

  ASSERT_NE(nullptr, work_queue_->GetFrontTask());
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());

  ASSERT_NE(nullptr, work_queue_->GetBackTask());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, SwapLockedAfterFenceHit) {
  work_queue_->InsertFence(1);
  incoming_queue_->push(FakeTaskWithEnqueueOrder(2));
  incoming_queue_->push(FakeTaskWithEnqueueOrder(3));
  incoming_queue_->push(FakeTaskWithEnqueueOrder(4));

  WorkQueue* work_queue;
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_TRUE(work_queue_->Empty());
  work_queue_->SwapLocked(*incoming_queue_.get());

  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_TRUE(incoming_queue_->empty());

  ASSERT_NE(nullptr, work_queue_->GetFrontTask());
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());

  ASSERT_NE(nullptr, work_queue_->GetBackTask());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, TakeTaskFromWorkQueue) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  WorkQueue* work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(3ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(4ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());

  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_TRUE(work_queue_->Empty());
}

TEST_F(WorkQueueTest, TakeTaskFromWorkQueue_HitFence) {
  work_queue_->InsertFence(3);
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  WorkQueue* work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, InsertFenceBeforeEnqueueing) {
  EXPECT_FALSE(work_queue_->InsertFence(1));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  EnqueueOrder enqueue_order;
  EXPECT_FALSE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
}

TEST_F(WorkQueueTest, InsertFenceAfterEnqueueingNonBlocking) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  EXPECT_FALSE(work_queue_->InsertFence(5));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EnqueueOrder enqueue_order;
  EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
}

TEST_F(WorkQueueTest, InsertFenceAfterEnqueueing) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  // NB in reality a fence will always be greater than any currently enqueued
  // tasks.
  EXPECT_FALSE(work_queue_->InsertFence(1));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EnqueueOrder enqueue_order;
  EXPECT_FALSE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
}

TEST_F(WorkQueueTest, InsertNewFence) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  work_queue_->Push(FakeTaskWithEnqueueOrder(5));

  EXPECT_FALSE(work_queue_->InsertFence(3));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  // Note until TakeTaskFromWorkQueue() is called we don't hit the fence.
  EnqueueOrder enqueue_order;
  EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_EQ(2ull, enqueue_order);

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_FALSE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  // Inserting the new fence should temporarily unblock the queue until the new
  // one is hit.
  EXPECT_TRUE(work_queue_->InsertFence(6));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_EQ(4ull, enqueue_order);
  EXPECT_EQ(4ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, PushWithNonEmptyQueueDoesNotHitFence) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, RemoveFence) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  work_queue_->Push(FakeTaskWithEnqueueOrder(5));
  work_queue_->InsertFence(3);

  WorkQueue* work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->RemoveFence());
  EXPECT_EQ(4ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, RemoveFenceButNoFence) {
  EXPECT_FALSE(work_queue_->RemoveFence());
}

TEST_F(WorkQueueTest, RemoveFenceNothingUnblocked) {
  EXPECT_FALSE(work_queue_->InsertFence(1));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_FALSE(work_queue_->RemoveFence());
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFence) {
  EXPECT_FALSE(work_queue_->BlockedByFence());
  EXPECT_FALSE(work_queue_->InsertFence(1));
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFencePopBecomesEmpty) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(2));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(1ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFencePop) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(2));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(1ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, InitiallyEmptyBlockedByFenceNewFenceUnblocks) {
  EXPECT_FALSE(work_queue_->InsertFence(1));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  EXPECT_TRUE(work_queue_->InsertFence(3));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFenceNewFenceUnblocks) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(2));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(1ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->InsertFence(4));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, InsertFenceAfterEnqueuing) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_FALSE(work_queue_->InsertFence(1));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EnqueueOrder enqueue_order;
  EXPECT_FALSE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
