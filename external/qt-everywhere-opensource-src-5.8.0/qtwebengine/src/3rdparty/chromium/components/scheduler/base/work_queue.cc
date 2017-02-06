// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/work_queue.h"

#include "components/scheduler/base/work_queue_sets.h"

namespace scheduler {
namespace internal {

WorkQueue::WorkQueue(TaskQueueImpl* task_queue, const char* name)
    : work_queue_sets_(nullptr),
      task_queue_(task_queue),
      work_queue_set_index_(0),
      name_(name) {}

void WorkQueue::AsValueInto(base::trace_event::TracedValue* state) const {
  std::queue<TaskQueueImpl::Task> queue_copy(work_queue_);
  while (!queue_copy.empty()) {
    TaskQueueImpl::TaskAsValueInto(queue_copy.front(), state);
    queue_copy.pop();
  }
}

WorkQueue::~WorkQueue() {
  DCHECK(!work_queue_sets_) << task_queue_ ->GetName() << " : "
      << work_queue_sets_->name() << " : " << name_;
}

const TaskQueueImpl::Task* WorkQueue::GetFrontTask() const {
  if (work_queue_.empty())
    return nullptr;
  return &work_queue_.front();
}

bool WorkQueue::GetFrontTaskEnqueueOrder(EnqueueOrder* enqueue_order) const {
  if (work_queue_.empty())
    return false;
  *enqueue_order = work_queue_.front().enqueue_order();
  return true;
}

void WorkQueue::Push(const TaskQueueImpl::Task& task) {
  bool was_empty = work_queue_.empty();
  work_queue_.push(task);
  if (was_empty && work_queue_sets_)
    work_queue_sets_->OnPushQueue(this);
}

void WorkQueue::PushAndSetEnqueueOrder(const TaskQueueImpl::Task& task,
                                       EnqueueOrder enqueue_order) {
  bool was_empty = work_queue_.empty();
  work_queue_.push(task);
  work_queue_.back().set_enqueue_order(enqueue_order);

  if (was_empty && work_queue_sets_)
    work_queue_sets_->OnPushQueue(this);
}

void WorkQueue::PopTaskForTest() {
  work_queue_.pop();
}

void WorkQueue::SwapLocked(std::queue<TaskQueueImpl::Task>& incoming_queue) {
  std::swap(work_queue_, incoming_queue);

  if (!work_queue_.empty() && work_queue_sets_)
    work_queue_sets_->OnPushQueue(this);
  task_queue_->TraceQueueSize(true);
}

TaskQueueImpl::Task WorkQueue::TakeTaskFromWorkQueue() {
  DCHECK(work_queue_sets_);
  DCHECK(!work_queue_.empty());
  TaskQueueImpl::Task pending_task = std::move(work_queue_.front());
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
