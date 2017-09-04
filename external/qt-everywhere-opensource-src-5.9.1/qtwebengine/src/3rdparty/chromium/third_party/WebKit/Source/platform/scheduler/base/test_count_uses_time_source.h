// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TEST_COUNT_USES_TIME_SOURCE_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TEST_COUNT_USES_TIME_SOURCE_H_

#include "base/macros.h"
#include "base/time/tick_clock.h"

namespace blink {
namespace scheduler {

class TestCountUsesTimeSource : public base::TickClock {
 public:
  explicit TestCountUsesTimeSource();
  ~TestCountUsesTimeSource() override;

  base::TimeTicks NowTicks() override;
  int now_calls_count() { return now_calls_count_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCountUsesTimeSource);

  int now_calls_count_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TEST_COUNT_USES_TIME_SOURCE_H_
