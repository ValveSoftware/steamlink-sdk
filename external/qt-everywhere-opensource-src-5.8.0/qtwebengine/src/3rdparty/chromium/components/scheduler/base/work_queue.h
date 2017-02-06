// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_BASE_WORK_QUEUE_H_
#define CONTENT_RENDERER_SCHEDULER_BASE_WORK_QUEUE_H_

#include <stddef.h>

#include <set>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/base/enqueue_order.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {
namespace internal {
class WorkQueueSets;

class SCHEDULER_EXPORT WorkQueue {
 public:
  WorkQueue(TaskQueueImpl* task_queue, const char* name);
  ~WorkQueue();

  // Associates this work queue with the given work queue sets. This must be
  // called before any tasks can be inserted into this work queue.
  void AssignToWorkQueueSets(WorkQueueSets* work_queue_sets);

  // Assigns the current set index.
  void AssignSetIndex(size_t work_queue_set_index);

  void AsValueInto(base::trace_event::TracedValue* state) const;

  // Clears the |work_queue_|.
  void Clear();

  // returns true if the |work_queue_| is empty.
  bool Empty() const { return work_queue_.empty(); }

  // If the |work_queue_| isn't empty, |enqueue_order| gets set to the enqueue
  // order of the front task and the function returns true.  Otherwise the
  // function returns false.
  bool GetFrontTaskEnqueueOrder(EnqueueOrder* enqueue_order) const;

  // Returns the first task in this queue or null if the queue is empty.
  const TaskQueueImpl::Task* GetFrontTask() const;

  // Pushes the task onto the |work_queue_| and informs the WorkQueueSets if
  // the head changed.
  void Push(const TaskQueueImpl::Task& task);

  // Pushes the task onto the |work_queue_|, sets the |enqueue_order| and
  // informs the WorkQueueSets if the head changed.
  void PushAndSetEnqueueOrder(const TaskQueueImpl::Task& task,
                              EnqueueOrder enqueue_order);

  // Swap the |work_queue_| with |incoming_queue| and informs the
  // WorkQueueSets if the head changed. Assumes |task_queue_->any_thread_lock_|
  // is locked.
  void SwapLocked(std::queue<TaskQueueImpl::Task>& incoming_queue);

  size_t Size() const { return work_queue_.size(); }

  // Pulls a task off the |work_queue_| and informs the WorkQueueSets.
  TaskQueueImpl::Task TakeTaskFromWorkQueue();

  const char* name() const { return name_; }

  TaskQueueImpl* task_queue() const { return task_queue_; }

  WorkQueueSets* work_queue_sets() const { return work_queue_sets_; }

  size_t work_queue_set_index() const { return work_queue_set_index_; }

  // Test support function. This should not be used in production code.
  void PopTaskForTest();

  // Returns true if the front task in this queue has an older enqueue order
  // than the front task of |other_queue|. Both queue are assumed to be
  // non-empty.
  bool ShouldRunBefore(const WorkQueue* other_queue) const;

 private:
  std::queue<TaskQueueImpl::Task> work_queue_;
  WorkQueueSets* work_queue_sets_;  // NOT OWNED.
  TaskQueueImpl* task_queue_;       // NOT OWNED.
  size_t work_queue_set_index_;
  const char* name_;
};

}  // namespace internal
}  // namespace scheduler

#endif  // CONTENT_RENDERER_SCHEDULER_BASE_WORK_QUEUE_H_
