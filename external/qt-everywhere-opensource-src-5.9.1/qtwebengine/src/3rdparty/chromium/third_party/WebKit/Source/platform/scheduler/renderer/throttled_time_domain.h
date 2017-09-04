// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_THROTTLED_TIME_DOMAIN_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_THROTTLED_TIME_DOMAIN_H_

#include "base/macros.h"
#include "platform/scheduler/base/real_time_domain.h"

namespace blink {
namespace scheduler {

// A time domain for throttled tasks. behaves like an RealTimeDomain except it
// relies on the owner (TaskQueueThrottler) to schedule wakeups.
class BLINK_PLATFORM_EXPORT ThrottledTimeDomain : public RealTimeDomain {
 public:
  ThrottledTimeDomain(TimeDomain::Observer* observer,
                      const char* tracing_category);
  ~ThrottledTimeDomain() override;

  // TimeDomain implementation:
  const char* GetName() const override;
  void RequestWakeup(base::TimeTicks now, base::TimeDelta delay) override;
  bool MaybeAdvanceTime() override;

  using TimeDomain::WakeupReadyDelayedQueues;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThrottledTimeDomain);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_THROTTLED_TIME_DOMAIN_H_
