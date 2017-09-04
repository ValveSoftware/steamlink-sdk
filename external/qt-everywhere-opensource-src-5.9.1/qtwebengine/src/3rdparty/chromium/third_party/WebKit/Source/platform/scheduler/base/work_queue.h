// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_WORK_QUEUE_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_WORK_QUEUE_H_

#include <stddef.h>

#include <set>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "platform/scheduler/base/enqueue_order.h"
#include "platform/scheduler/base/intrusive_heap.h"
#include "platform/scheduler/base/task_queue_impl.h"

namespace blink {
namespace scheduler {
namespace internal {
class WorkQueueSets;

// This class keeps track of immediate and delayed tasks which are due to run
// now. It interfaces deeply with WorkQueueSets which keeps track of which queue
// (with a given priority) contains the oldest task.
//
// If a fence is inserted, WorkQueue behaves normally up until
// TakeTaskFromWorkQueue reaches or exceeds the fence.  At that point it the
// API subset used by WorkQueueSets pretends the WorkQueue is empty until the
// fence is removed.  This functionality is a primitive intended for use by
// throttling mechanisms.
class BLINK_PLATFORM_EXPORT WorkQueue {
 public:
  WorkQueue(TaskQueueImpl* task_queue, const char* name);
  ~WorkQueue();

  // Associates this work queue with the given work queue sets. This must be
  // called before any tasks can be inserted into this work queue.
  void AssignToWorkQueueSets(WorkQueueSets* work_queue_sets);

  // Assigns the current set index.
  void AssignSetIndex(size_t work_queue_set_index);

  void AsValueInto(base::trace_event::TracedValue* state) const;

  // Returns true if the |work_queue_| is empty. This method ignores any fences.
  bool Empty() const { return work_queue_.empty(); }

  // If the |work_queue_| isn't empty and a fence hasn't been reached,
  // |enqueue_order| gets set to the enqueue order of the front task and the
  // function returns true.  Otherwise the function returns false.
  bool GetFrontTaskEnqueueOrder(EnqueueOrder* enqueue_order) const;

  // Returns the first task in this queue or null if the queue is empty. This
  // method ignores any fences.
  const TaskQueueImpl::Task* GetFrontTask() const;

  // Returns the last task in this queue or null if the queue is empty. This
  // method ignores any fences.
  const TaskQueueImpl::Task* GetBackTask() const;

  // Pushes the task onto the |work_queue_| and a fence hasn't been reached it
  // informs the WorkQueueSets if the head changed.
  void Push(TaskQueueImpl::Task task);

  // Swap the |work_queue_| with |incoming_queue| and if a fence hasn't been
  // reached it informs the WorkQueueSets if the head changed. Assumes
  // |task_queue_->any_thread_lock_| is locked.
  void SwapLocked(std::queue<TaskQueueImpl::Task>& incoming_queue);

  size_t Size() const { return work_queue_.size(); }

  // Pulls a task off the |work_queue_| and informs the WorkQueueSets.  If the
  // task removed had an enqueue order >= the current fence then WorkQueue
  // pretends to be empty as far as the WorkQueueSets is concrned.
  TaskQueueImpl::Task TakeTaskFromWorkQueue();

  const char* name() const { return name_; }

  TaskQueueImpl* task_queue() const { return task_queue_; }

  WorkQueueSets* work_queue_sets() const { return work_queue_sets_; }

  size_t work_queue_set_index() const { return work_queue_set_index_; }

  HeapHandle heap_handle() const { return heap_handle_; }

  void set_heap_handle(HeapHandle handle) { heap_handle_ = handle; }

  // Test support function. This should not be used in production code.
  void PopTaskForTest();

  // Returns true if the front task in this queue has an older enqueue order
  // than the front task of |other_queue|. Both queue are assumed to be
  // non-empty. This method ignores any fences.
  bool ShouldRunBefore(const WorkQueue* other_queue) const;

  // Submit a fence. When TakeTaskFromWorkQueue encounters a task whose
  // enqueue_order is >= |fence| then the WorkQueue will start pretending to be.
  // empty.
  // Inserting a fence may supersede a previous one and unblock some tasks.
  // Returns true if any tasks where unblocked, returns false otherwise.
  bool InsertFence(EnqueueOrder fence);

  // Removes any fences that where added and if WorkQueue was pretending to be
  // empty, then the real value is reported to WorkQueueSets. Returns true if
  // any tasks where unblocked.
  bool RemoveFence();

  // Returns true if any tasks are blocked by the fence. Returns true if the
  // queue is empty and fence has been set (i.e. future tasks would be blocked).
  // Otherwise returns false.
  bool BlockedByFence() const;

 private:
  std::queue<TaskQueueImpl::Task> work_queue_;
  WorkQueueSets* work_queue_sets_;  // NOT OWNED.
  TaskQueueImpl* task_queue_;       // NOT OWNED.
  size_t work_queue_set_index_;
  HeapHandle heap_handle_;
  const char* name_;
  EnqueueOrder fence_;

  DISALLOW_COPY_AND_ASSIGN(WorkQueue);
};

}  // namespace internal
}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_WORK_QUEUE_H_
