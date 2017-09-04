// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_SCHEDULER_TQM_DELEGATE_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_SCHEDULER_TQM_DELEGATE_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "platform/scheduler/base/task_queue_manager_delegate.h"

namespace blink {
namespace scheduler {

class BLINK_PLATFORM_EXPORT SchedulerTqmDelegate
    : public TaskQueueManagerDelegate {
 public:
  SchedulerTqmDelegate() {}

  // If the underlying task runner supports the concept of a default task
  // runner, the delegate should implement this function to redirect that task
  // runner to the scheduler.
  virtual void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) = 0;

  // Similarly this method can be used to restore the original task runner when
  // the scheduler no longer wants to intercept tasks.
  virtual void RestoreDefaultTaskRunner() = 0;

 protected:
  ~SchedulerTqmDelegate() override {}

  DISALLOW_COPY_AND_ASSIGN(SchedulerTqmDelegate);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_SCHEDULER_TQM_DELEGATE_H_
