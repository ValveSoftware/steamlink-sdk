// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_
#define COMPONENTS_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_

#include <set>

#include "base/macros.h"
#include "components/scheduler/base/time_domain.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SCHEDULER_EXPORT RealTimeDomain : public TimeDomain {
 public:
  explicit RealTimeDomain(const char* tracing_category);
  RealTimeDomain(TimeDomain::Observer* observer, const char* tracing_category);
  ~RealTimeDomain() override;

  // TimeDomain implementation:
  LazyNow CreateLazyNow() const override;
  base::TimeTicks Now() const override;
  bool MaybeAdvanceTime() override;
  const char* GetName() const override;

 protected:
  void OnRegisterWithTaskQueueManager(
      TaskQueueManager* task_queue_manager) override;
  void RequestWakeup(base::TimeTicks now, base::TimeDelta delay) override;
  void AsValueIntoInternal(
      base::trace_event::TracedValue* state) const override;

 private:
  const char* tracing_category_;          // NOT OWNED
  TaskQueueManager* task_queue_manager_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(RealTimeDomain);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_
