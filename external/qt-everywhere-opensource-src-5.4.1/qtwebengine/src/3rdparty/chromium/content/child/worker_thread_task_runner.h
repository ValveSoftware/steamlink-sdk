// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WORKER_THREAD_TASK_RUNNER_H_
#define CONTENT_CHILD_WORKER_THREAD_TASK_RUNNER_H_

#include "base/task_runner.h"

namespace content {

// A task runner that runs tasks on a single webkit worker thread which
// is managed by WorkerTaskRunner.
// Note that this implementation ignores the delay duration for PostDelayedTask
// and have it behave the same as PostTask.
class WorkerThreadTaskRunner : public base::TaskRunner {
 public:
  explicit WorkerThreadTaskRunner(int worker_thread_id);

  // Gets the WorkerThreadTaskRunner for the current worker thread.
  // This returns non-null value only when it is called on a worker thread.
  static scoped_refptr<WorkerThreadTaskRunner> current();

  // TaskRunner overrides.
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE;
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE;

 protected:
  virtual ~WorkerThreadTaskRunner();

 private:
  const int worker_thread_id_;
};

}  // namespace content

#endif  // CONTENT_CHILD_WORKER_THREAD_TASK_RUNNER_H_
