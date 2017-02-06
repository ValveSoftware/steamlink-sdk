// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/throttled_time_domain.h"

namespace scheduler {

ThrottledTimeDomain::ThrottledTimeDomain(TimeDomain::Observer* observer,
                                         const char* tracing_category)
    : RealTimeDomain(observer, tracing_category) {}

ThrottledTimeDomain::~ThrottledTimeDomain() {}

const char* ThrottledTimeDomain::GetName() const {
  return "ThrottledTimeDomain";
}

void ThrottledTimeDomain::RequestWakeup(base::TimeTicks now,
                                        base::TimeDelta delay) {
  // We assume the owner (i.e. ThrottlingHelper) will manage wakeups on our
  // behalf.
}

bool ThrottledTimeDomain::MaybeAdvanceTime() {
  base::TimeTicks next_run_time;
  if (!NextScheduledRunTime(&next_run_time))
    return false;

  base::TimeTicks now = Now();
  if (now >= next_run_time)
    return true;  // Causes DoWork to post a continuation.

  // Unlike RealTimeDomain::MaybeAdvanceTime we don't request a wake up here, we
  // assume the owner (i.e. ThrottlingHelper) will manage wakeups on our behalf.
  return false;
}

}  // namespace scheduler
