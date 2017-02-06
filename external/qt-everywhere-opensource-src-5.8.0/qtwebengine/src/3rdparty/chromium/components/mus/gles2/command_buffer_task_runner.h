// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_COMMAND_BUFFER_TASK_RUNNER_H_
#define COMPONENTS_MUS_GLES2_COMMAND_BUFFER_TASK_RUNNER_H_

#include <deque>
#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"

namespace mus {

class CommandBufferDriver;

// This class maintains tasks submitted by |CommandBufferImpl|. Those tasks will
// be executed on the main thread. But if the main thread is blocked in
// |CommandBufferLocal::OnWaitFenceSync()| by waiting a sync point, the
// |CommandBufferTaskRunner::RunOneTask()| could be used for executing a task
// from other command buffers which may retire the sync point.
class CommandBufferTaskRunner
    : public base::RefCountedThreadSafe<CommandBufferTaskRunner> {
 public:
  CommandBufferTaskRunner();

  // TaskCallback returns true if the task is completed and should be removed
  // from the task queue, otherwise returns false.
  typedef base::Callback<bool(void)> TaskCallback;
  bool PostTask(const CommandBufferDriver* driver,
                const TaskCallback& task);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

 private:
  friend class base::RefCountedThreadSafe<CommandBufferTaskRunner>;

  ~CommandBufferTaskRunner();

  // Run one command buffer task from a scheduled command buffer.
  // When there isn't any command buffer task, and if the |block| is false,
  // this function will return false immediately, otherwise, this function
  // will be blocked until a task is available for executing.
  bool RunOneTaskInternalLocked();

  // Post a task to the main thread to execute tasks in |driver_map_|, if it is
  // necessary.
  void ScheduleTaskIfNecessaryLocked();

  // The callback function for executing tasks in |driver_map_|.
  void RunCommandBufferTask();

  base::ThreadChecker thread_checker_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  typedef std::deque<TaskCallback> TaskQueue;
  typedef std::map<const CommandBufferDriver*, TaskQueue> DriverMap;
  DriverMap driver_map_;
  bool need_post_task_;

  // The access lock for |driver_map_| and |need_post_task_|.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferTaskRunner);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_COMMAND_BUFFER_TASK_RUNNER_H_
