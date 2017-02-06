// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/tick_clock_ref_counted.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/tick_clock.h"

namespace copresence {

TickClockRefCounted::TickClockRefCounted(std::unique_ptr<base::TickClock> clock)
    : clock_(std::move(clock)) {}

TickClockRefCounted::TickClockRefCounted(base::TickClock* clock)
    : clock_(base::WrapUnique(clock)) {}

base::TimeTicks TickClockRefCounted::NowTicks() const {
  return clock_->NowTicks();
}

TickClockRefCounted::~TickClockRefCounted() {}

}  // namespace copresence
