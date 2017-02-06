// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_SCHEDULER_TQM_DELEGATE_IMPL_H_
#define COMPONENTS_SCHEDULER_CHILD_SCHEDULER_TQM_DELEGATE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/tick_clock.h"
#include "components/scheduler/child/scheduler_tqm_delegate.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SCHEDULER_EXPORT SchedulerTqmDelegateImpl : public SchedulerTqmDelegate {
 public:
  // |message_loop| is not owned and must outlive the lifetime of this object.
  static scoped_refptr<SchedulerTqmDelegateImpl> Create(
      base::MessageLoop* message_loop,
      std::unique_ptr<base::TickClock> time_source);

  // SchedulerTqmDelegate implementation
  void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void RestoreDefaultTaskRunner() override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;
  bool IsNested() const override;
  base::TimeTicks NowTicks() override;

 protected:
  ~SchedulerTqmDelegateImpl() override;

 private:
  SchedulerTqmDelegateImpl(base::MessageLoop* message_loop,
                           std::unique_ptr<base::TickClock> time_source);

  // Not owned.
  base::MessageLoop* message_loop_;
  scoped_refptr<SingleThreadTaskRunner> message_loop_task_runner_;
  std::unique_ptr<base::TickClock> time_source_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerTqmDelegateImpl);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_SCHEDULER_TQM_DELEGATE_IMPL_H_
