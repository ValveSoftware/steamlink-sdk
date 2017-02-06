// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_AUTO_ADVANCING_VIRTUAL_TIME_DOMAIN_H_
#define COMPONENTS_SCHEDULER_RENDERER_AUTO_ADVANCING_VIRTUAL_TIME_DOMAIN_H_

#include "base/macros.h"
#include "components/scheduler/base/virtual_time_domain.h"

namespace scheduler {

// A time domain that runs tasks sequentially in time order but doesn't sleep
// between delayed tasks.
//
// KEY: A-E are delayed tasks
// |    A   B C  D           E  (Execution with RealTimeDomain)
// |-----------------------------> time
//
// |ABCDE                       (Execution with AutoAdvancingVirtualTimeDomain)
// |-----------------------------> time
class SCHEDULER_EXPORT AutoAdvancingVirtualTimeDomain
    : public VirtualTimeDomain {
 public:
  explicit AutoAdvancingVirtualTimeDomain(base::TimeTicks initial_time);
  ~AutoAdvancingVirtualTimeDomain() override;

  // TimeDomain implementation:
  bool MaybeAdvanceTime() override;
  void RequestWakeup(base::TimeTicks now, base::TimeDelta delay) override;
  const char* GetName() const override;

  // Controls whether or not virtual time is allowed to advance, when the
  // TaskQueueManager runs out of immediate work to do.
  void SetCanAdvanceVirtualTime(bool can_advance_virtual_time);

 private:
  bool can_advance_virtual_time_;

  DISALLOW_COPY_AND_ASSIGN(AutoAdvancingVirtualTimeDomain);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_AUTO_ADVANCING_VIRTUAL_TIME_DOMAIN_H_
