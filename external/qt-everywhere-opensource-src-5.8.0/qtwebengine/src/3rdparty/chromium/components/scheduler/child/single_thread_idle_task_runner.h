// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_SINGLE_THREAD_IDLE_TASK_RUNNER_H_
#define COMPONENTS_SCHEDULER_CHILD_SINGLE_THREAD_IDLE_TASK_RUNNER_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
namespace trace_event {
class BlameContext;
}
}

namespace scheduler {

// A SingleThreadIdleTaskRunner is a task runner for running idle tasks. Idle
// tasks have an unbound argument which is bound to a deadline
// (in base::TimeTicks) when they are run. The idle task is expected to
// complete by this deadline.
class SingleThreadIdleTaskRunner
    : public base::RefCountedThreadSafe<SingleThreadIdleTaskRunner> {
 public:
  typedef base::Callback<void(base::TimeTicks)> IdleTask;

  // Used to request idle task deadlines and signal posting of idle tasks.
  class SCHEDULER_EXPORT Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // Signals that an idle task has been posted. This will be called on the
    // posting thread, which may not be the same thread as the
    // SingleThreadIdleTaskRunner runs on.
    virtual void OnIdleTaskPosted() = 0;

    // Signals that a new idle task is about to be run and returns the deadline
    // for this idle task.
    virtual base::TimeTicks WillProcessIdleTask() = 0;

    // Signals that an idle task has finished being run.
    virtual void DidProcessIdleTask() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // NOTE Category strings must have application lifetime (statics or
  // literals). They may not include " chars.
  SingleThreadIdleTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> idle_priority_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> after_wakeup_task_runner,
      Delegate* Delegate,
      const char* tracing_category);

  virtual void PostIdleTask(const tracked_objects::Location& from_here,
                            const IdleTask& idle_task);

  virtual void PostNonNestableIdleTask(
      const tracked_objects::Location& from_here,
      const IdleTask& idle_task);

  virtual void PostIdleTaskAfterWakeup(
      const tracked_objects::Location& from_here,
      const IdleTask& idle_task);

  bool RunsTasksOnCurrentThread() const;

  void SetBlameContext(base::trace_event::BlameContext* blame_context);

 protected:
  virtual ~SingleThreadIdleTaskRunner();

 private:
  friend class base::RefCountedThreadSafe<SingleThreadIdleTaskRunner>;

  void RunTask(IdleTask idle_task);

  scoped_refptr<base::SingleThreadTaskRunner> idle_priority_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> after_wakeup_task_runner_;
  Delegate* delegate_;  // NOT OWNED
  const char* tracing_category_;
  base::trace_event::BlameContext* blame_context_;  // Not owned.
  base::WeakPtr<SingleThreadIdleTaskRunner> weak_scheduler_ptr_;
  base::WeakPtrFactory<SingleThreadIdleTaskRunner> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(SingleThreadIdleTaskRunner);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_SINGLE_THREAD_IDLE_TASK_RUNNER_H_
