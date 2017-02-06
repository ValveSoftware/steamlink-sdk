// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_THROTTLED_TIME_DOMAIN_H_
#define COMPONENTS_SCHEDULER_RENDERER_THROTTLED_TIME_DOMAIN_H_

#include "base/macros.h"
#include "components/scheduler/base/real_time_domain.h"

namespace scheduler {

// A time domain for throttled tasks. behaves like an RealTimeDomain except it
// relies on the owner (ThrottlingHelper) to schedule wakeups.
class SCHEDULER_EXPORT ThrottledTimeDomain : public RealTimeDomain {
 public:
  ThrottledTimeDomain(TimeDomain::Observer* observer,
                      const char* tracing_category);
  ~ThrottledTimeDomain() override;

  // TimeDomain implementation:
  const char* GetName() const override;
  void RequestWakeup(base::TimeTicks now, base::TimeDelta delay) override;
  bool MaybeAdvanceTime() override;

  using TimeDomain::ClearExpiredWakeups;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThrottledTimeDomain);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_THROTTLED_TIME_DOMAIN_H_
