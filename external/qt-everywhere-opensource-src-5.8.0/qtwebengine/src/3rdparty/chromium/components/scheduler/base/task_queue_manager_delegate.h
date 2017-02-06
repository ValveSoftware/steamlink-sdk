// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_H_
#define COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/tick_clock.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SCHEDULER_EXPORT TaskQueueManagerDelegate
    : public base::SingleThreadTaskRunner,
      public base::TickClock {
 public:
  TaskQueueManagerDelegate() {}

  // Returns true if the task runner is nested (i.e., running a run loop within
  // a nested task).
  virtual bool IsNested() const = 0;

 protected:
  ~TaskQueueManagerDelegate() override {}

  DISALLOW_COPY_AND_ASSIGN(TaskQueueManagerDelegate);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_H_
