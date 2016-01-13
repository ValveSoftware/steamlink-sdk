// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/blocking_task_runner.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_proxy.h"

namespace cc {

struct TaskRunnerPairs {
  static TaskRunnerPairs* GetInstance() {
    return Singleton<TaskRunnerPairs>::get();
  }

  base::Lock lock;
  std::vector<scoped_refptr<BlockingTaskRunner> > runners;

 private:
  friend struct DefaultSingletonTraits<TaskRunnerPairs>;
};

// static
scoped_refptr<BlockingTaskRunner> BlockingTaskRunner::current() {
  TaskRunnerPairs* task_runners = TaskRunnerPairs::GetInstance();
  base::PlatformThreadId thread_id = base::PlatformThread::CurrentId();

  base::AutoLock lock(task_runners->lock);

  scoped_refptr<BlockingTaskRunner> current_task_runner;

  for (size_t i = 0; i < task_runners->runners.size(); ++i) {
    if (task_runners->runners[i]->thread_id_ == thread_id) {
      current_task_runner = task_runners->runners[i];
    } else if (task_runners->runners[i]->HasOneRef()) {
      task_runners->runners.erase(task_runners->runners.begin() + i);
      i--;
    }
  }

  if (current_task_runner)
    return current_task_runner;

  scoped_refptr<BlockingTaskRunner> runner =
      new BlockingTaskRunner(base::MessageLoopProxy::current());
  task_runners->runners.push_back(runner);
  return runner;
}

BlockingTaskRunner::BlockingTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : thread_id_(base::PlatformThread::CurrentId()),
      task_runner_(task_runner),
      capture_(0) {
}

BlockingTaskRunner::~BlockingTaskRunner() {}

bool BlockingTaskRunner::BelongsToCurrentThread() {
  return base::PlatformThread::CurrentId() == thread_id_;
}

bool BlockingTaskRunner::PostTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task) {
  base::AutoLock lock(lock_);
  DCHECK(task_runner_.get() || capture_);
  if (!capture_)
    return task_runner_->PostTask(from_here, task);
  captured_tasks_.push_back(task);
  return true;
}

void BlockingTaskRunner::SetCapture(bool capture) {
  DCHECK(BelongsToCurrentThread());

  std::vector<base::Closure> tasks;

  {
    base::AutoLock lock(lock_);
    capture_ += capture ? 1 : -1;
    DCHECK_GE(capture_, 0);

    if (capture_)
      return;

    // We're done capturing, so grab all the captured tasks and run them.
    tasks.swap(captured_tasks_);
  }
  for (size_t i = 0; i < tasks.size(); ++i)
    tasks[i].Run();
}

BlockingTaskRunner::CapturePostTasks::CapturePostTasks()
    : blocking_runner_(BlockingTaskRunner::current()) {
  blocking_runner_->SetCapture(true);
}

BlockingTaskRunner::CapturePostTasks::~CapturePostTasks() {
  blocking_runner_->SetCapture(false);
}

}  // namespace cc
