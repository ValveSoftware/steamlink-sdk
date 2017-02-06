// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_task_runner.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/mus/gles2/command_buffer_driver.h"

namespace mus {

CommandBufferTaskRunner::CommandBufferTaskRunner()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      need_post_task_(true) {
}

bool CommandBufferTaskRunner::PostTask(
    const CommandBufferDriver* driver,
    const TaskCallback& task) {
  base::AutoLock locker(lock_);
  driver_map_[driver].push_back(task);
  ScheduleTaskIfNecessaryLocked();
  return true;
}

CommandBufferTaskRunner::~CommandBufferTaskRunner() {}

bool CommandBufferTaskRunner::RunOneTaskInternalLocked() {
  DCHECK(thread_checker_.CalledOnValidThread());
  lock_.AssertAcquired();

  for (auto it = driver_map_.begin(); it != driver_map_.end(); ++it) {
    if (!it->first->IsScheduled())
      continue;
    TaskQueue& task_queue = it->second;
    DCHECK(!task_queue.empty());
    const TaskCallback& callback = task_queue.front();
    bool complete = false;
    {
      base::AutoUnlock unlocker(lock_);
      complete = callback.Run();
    }
    if (complete) {
      // Only remove the task if it is complete.
      task_queue.pop_front();
      if (task_queue.empty())
        driver_map_.erase(it);
    }
    return true;
  }
  return false;
}

void CommandBufferTaskRunner::ScheduleTaskIfNecessaryLocked() {
  lock_.AssertAcquired();
  if (!need_post_task_)
    return;
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CommandBufferTaskRunner::RunCommandBufferTask, this));
  need_post_task_ = false;
}

void CommandBufferTaskRunner::RunCommandBufferTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock locker(lock_);

  // Try executing all tasks in |driver_map_| until |RunOneTaskInternalLock()|
  // returns false (Returning false means |driver_map_| is empty or all tasks
  // in it aren't executable currently).
  while (RunOneTaskInternalLocked())
    ;
  need_post_task_ = true;
}

}  // namespace mus
