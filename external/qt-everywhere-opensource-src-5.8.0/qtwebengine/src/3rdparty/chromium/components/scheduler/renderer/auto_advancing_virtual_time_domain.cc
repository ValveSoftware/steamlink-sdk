// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/auto_advancing_virtual_time_domain.h"

namespace scheduler {

AutoAdvancingVirtualTimeDomain::AutoAdvancingVirtualTimeDomain(
    base::TimeTicks initial_time)
    : VirtualTimeDomain(nullptr, initial_time),
      can_advance_virtual_time_(true) {}

AutoAdvancingVirtualTimeDomain::~AutoAdvancingVirtualTimeDomain() {}

bool AutoAdvancingVirtualTimeDomain::MaybeAdvanceTime() {
  base::TimeTicks run_time;
  if (!can_advance_virtual_time_ || !NextScheduledRunTime(&run_time)) {
    return false;
  }
  AdvanceTo(run_time);
  return true;
}

void AutoAdvancingVirtualTimeDomain::RequestWakeup(base::TimeTicks now,
                                                   base::TimeDelta delay) {
  base::TimeTicks dummy;
  if (can_advance_virtual_time_ && !NextScheduledRunTime(&dummy))
    RequestDoWork();
}

void AutoAdvancingVirtualTimeDomain::SetCanAdvanceVirtualTime(
    bool can_advance_virtual_time) {
  can_advance_virtual_time_ = can_advance_virtual_time;
  if (can_advance_virtual_time_)
    RequestDoWork();
}

const char* AutoAdvancingVirtualTimeDomain::GetName() const {
  return "AutoAdvancingVirtualTimeDomain";
}

}  // namespace scheduler
