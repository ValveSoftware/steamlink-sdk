// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_VIRTUAL_TIME_DOMAIN_H_
#define COMPONENTS_SCHEDULER_BASE_VIRTUAL_TIME_DOMAIN_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/scheduler/base/time_domain.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SCHEDULER_EXPORT VirtualTimeDomain : public TimeDomain {
 public:
  VirtualTimeDomain(TimeDomain::Observer* observer,
                    base::TimeTicks initial_time);
  ~VirtualTimeDomain() override;

  // TimeDomain implementation:
  LazyNow CreateLazyNow() const override;
  base::TimeTicks Now() const override;
  bool MaybeAdvanceTime() override;
  const char* GetName() const override;

  // Advances this time domain to |now|. NOTE |now| is supposed to be
  // monotonically increasing.  NOTE it's the responsibility of the caller to
  // call TaskQueueManager::MaybeScheduleImmediateWork if needed.
  void AdvanceTo(base::TimeTicks now);

  using TimeDomain::ClearExpiredWakeups;

 protected:
  void OnRegisterWithTaskQueueManager(
      TaskQueueManager* task_queue_manager) override;
  void RequestWakeup(base::TimeTicks now, base::TimeDelta delay) override;
  void AsValueIntoInternal(
      base::trace_event::TracedValue* state) const override;

  void RequestDoWork();

 private:
  mutable base::Lock lock_;  // Protects |now_|.
  base::TimeTicks now_;

  TaskQueueManager* task_queue_manager_;  // NOT OWNED
  base::Closure do_work_closure_;

  DISALLOW_COPY_AND_ASSIGN(VirtualTimeDomain);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_VIRTUAL_TIME_DOMAIN_H_
