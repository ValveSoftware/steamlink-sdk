// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/work_queue.h"

#include "platform/scheduler/base/work_queue_sets.h"

namespace blink {
namespace scheduler {
namespace internal {

WorkQueue::WorkQueue(TaskQueueImpl* task_queue, const char* name)
    : work_queue_sets_(nullptr),
      task_queue_(task_queue),
      work_queue_set_index_(0),
      name_(name),
      fence_(0) {}

void WorkQueue::AsValueInto(base::trace_event::TracedValue* state) const {
  // Remove const to search |work_queue_| in the destructive manner. Restore the
  // content from |visited| later.
  std::queue<TaskQueueImpl::Task>* mutable_queue =
      const_cast<std::queue<TaskQueueImpl::Task>*>(&work_queue_);
  std::queue<TaskQueueImpl::Task> visited;
  while (!mutable_queue->empty()) {
    TaskQueueImpl::TaskAsValueInto(mutable_queue->front(), state);
    visited.push(std::move(mutable_queue->front()));
    mutable_queue->pop();
  }
  *mutable_queue = std::move(visited);
}

WorkQueue::~WorkQueue() {
  DCHECK(!work_queue_sets_) << task_queue_->GetName() << " : "
                            << work_queue_sets_->name() << " : " << name_;
}

const TaskQueueImpl::Task* WorkQueue::GetFrontTask() const {
  if (work_queue_.empty())
    return nullptr;
  return &work_queue_.front();
}

const TaskQueueImpl::Task* WorkQueue::GetBackTask() const {
  if (work_queue_.empty())
    return nullptr;
  return &work_queue_.back();
}

bool WorkQueue::BlockedByFence() const {
  if (!fence_)
    return false;

  // If the queue is empty then any future tasks will have a higher enqueue
  // order and will be blocked. The queue is also blocked if the head is past
  // the fence.
  return work_queue_.empty() || work_queue_.front().enqueue_order() > fence_;
}

bool WorkQueue::GetFrontTaskEnqueueOrder(EnqueueOrder* enqueue_order) const {
  if (work_queue_.empty() || BlockedByFence())
    return false;
  // Quick sanity check.
  DCHECK_LE(work_queue_.front().enqueue_order(),
            work_queue_.back().enqueue_order())
      << task_queue_->GetName() << " : " << work_queue_sets_->name() << " : "
      << name_;
  *enqueue_order = work_queue_.front().enqueue_order();
  return true;
}

void WorkQueue::Push(TaskQueueImpl::Task task) {
  bool was_empty = work_queue_.empty();
#ifndef NDEBUG
  DCHECK(task.enqueue_order_set());
#endif

  // Amoritized O(1).
  work_queue_.push(std::move(task));

  if (!was_empty)
    return;

  // If we hit the fence, pretend to WorkQueueSets that we're empty.
  if (work_queue_sets_ && !BlockedByFence())
    work_queue_sets_->OnPushQueue(this);
}

void WorkQueue::PopTaskForTest() {
  if (work_queue_.empty())
    return;
  work_queue_.pop();
}

void WorkQueue::SwapLocked(std::queue<TaskQueueImpl::Task>& incoming_queue) {
  DCHECK(work_queue_.empty());
  std::swap(work_queue_, incoming_queue);
  if (work_queue_.empty())
    return;
  // If we hit the fence, pretend to WorkQueueSets that we're empty.
  if (work_queue_sets_ && !BlockedByFence())
    work_queue_sets_->OnPushQueue(this);
}

TaskQueueImpl::Task WorkQueue::TakeTaskFromWorkQueue() {
  DCHECK(work_queue_sets_);
  DCHECK(!work_queue_.empty());

  // Skip over canceled tasks, except for the last one since we always return
  // something.
  while (work_queue_.size() > 1u && work_queue_.front().task.IsCancelled()) {
    work_queue_.pop();
  }

  TaskQueueImpl::Task pending_task =
      std::move(const_cast<TaskQueueImpl::Task&>(work_queue_.front()));
  work_queue_.pop();
  work_queue_sets_->OnPopQueue(this);
  task_queue_->TraceQueueSize(false);
  return pending_task;
}

void WorkQueue::AssignToWorkQueueSets(WorkQueueSets* work_queue_sets) {
  work_queue_sets_ = work_queue_sets;
}

void WorkQueue::AssignSetIndex(size_t work_queue_set_index) {
  work_queue_set_index_ = work_queue_set_index;
}

bool WorkQueue::InsertFence(EnqueueOrder fence) {
  DCHECK_NE(fence, 0u);
  DCHECK(fence >= fence_ || fence == 1u);
  bool was_blocked_by_fence = BlockedByFence();
  fence_ = fence;
  // Moving the fence forward may unblock some tasks.
  if (work_queue_sets_ && !work_queue_.empty() && was_blocked_by_fence &&
      !BlockedByFence()) {
    work_queue_sets_->OnPushQueue(this);
    return true;
  }
  // Fence insertion may have blocked all tasks in this work queue.
  if (BlockedByFence())
    work_queue_sets_->OnQueueBlocked(this);
  return false;
}

bool WorkQueue::RemoveFence() {
  bool was_blocked_by_fence = BlockedByFence();
  fence_ = 0;
  if (work_queue_sets_ && !work_queue_.empty() && was_blocked_by_fence) {
    work_queue_sets_->OnPushQueue(this);
    return true;
  }
  return false;
}

bool WorkQueue::ShouldRunBefore(const WorkQueue* other_queue) const {
  DCHECK(!work_queue_.empty());
  DCHECK(!other_queue->work_queue_.empty());
  EnqueueOrder enqueue_order = 0;
  EnqueueOrder other_enqueue_order = 0;
  bool have_task = GetFrontTaskEnqueueOrder(&enqueue_order);
  bool have_other_task =
      other_queue->GetFrontTaskEnqueueOrder(&other_enqueue_order);
  DCHECK(have_task);
  DCHECK(have_other_task);
  return enqueue_order < other_enqueue_order;
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
