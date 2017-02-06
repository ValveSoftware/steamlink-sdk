// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_TEST_ALWAYS_FAIL_TIME_SOURCE_H_
#define COMPONENTS_SCHEDULER_BASE_TEST_ALWAYS_FAIL_TIME_SOURCE_H_

#include "base/macros.h"
#include "base/time/tick_clock.h"

namespace scheduler {

class TestAlwaysFailTimeSource : public base::TickClock {
 public:
  explicit TestAlwaysFailTimeSource();
  ~TestAlwaysFailTimeSource() override;

  base::TimeTicks NowTicks() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAlwaysFailTimeSource);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_TEST_ALWAYS_FAIL_TIME_SOURCE_H_
