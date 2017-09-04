// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"

namespace blink {
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
  // Avoid posting pointless DoWorks.  I.e. if the time domain has more then one
  // scheduled wake up then we don't need to do anything.
  if (can_advance_virtual_time_ && NumberOfScheduledWakeups() == 1u)
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
}  // namespace blink
