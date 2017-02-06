// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_TASK_SCHEDULER_H_
#define BASE_TASK_SCHEDULER_TASK_SCHEDULER_H_

#include <memory>

#include "base/base_export.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/task_scheduler/task_traits.h"

namespace tracked_objects {
class Location;
}

namespace base {

// Interface for a task scheduler and static methods to manage the instance used
// by the post_task.h API.
class BASE_EXPORT TaskScheduler {
 public:
  virtual ~TaskScheduler() = default;

  // Posts |task| with specific |traits|.
  // For one off tasks that don't require a TaskRunner.
  virtual void PostTaskWithTraits(const tracked_objects::Location& from_here,
                                  const TaskTraits& traits,
                                  const Closure& task) = 0;

  // Returns a TaskRunner whose PostTask invocations will result in scheduling
  // Tasks with |traits| which will be executed according to |execution_mode|.
  virtual scoped_refptr<TaskRunner> CreateTaskRunnerWithTraits(
      const TaskTraits& traits,
      ExecutionMode execution_mode) = 0;

  // Synchronously shuts down the scheduler. Once this is called, only tasks
  // posted with the BLOCK_SHUTDOWN behavior will be run. When this returns:
  // - All SKIP_ON_SHUTDOWN tasks that were already running have completed their
  //   execution.
  // - All posted BLOCK_SHUTDOWN tasks have completed their execution.
  // - CONTINUE_ON_SHUTDOWN tasks might still be running.
  // Note that an implementation can keep threads and other resources alive to
  // support running CONTINUE_ON_SHUTDOWN after this returns. This can only be
  // called once.
  virtual void Shutdown() = 0;

  // Registers |task_scheduler| to handle tasks posted through the post_task.h
  // API for this process. The registered TaskScheduler will only be deleted
  // when a new TaskScheduler is registered (i.e. otherwise leaked on shutdown).
  // This must not be called when TaskRunners created by the previous
  // TaskScheduler are still alive. This method is not thread-safe; proper
  // synchronization is required to use the post_task.h API after registering a
  // new TaskScheduler.
  static void SetInstance(std::unique_ptr<TaskScheduler> task_scheduler);

  // Retrieve the TaskScheduler set via SetInstance(). This should be used very
  // rarely; most users of TaskScheduler should use the post_task.h API.
  static TaskScheduler* GetInstance();
};

}  // namespace base

#endif  // BASE_TASK_SCHEDULER_TASK_SCHEDULER_H_
