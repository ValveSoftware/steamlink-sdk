// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/test_count_uses_time_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

TestCountUsesTimeSource::TestCountUsesTimeSource() : now_calls_count_(0) {}

TestCountUsesTimeSource::~TestCountUsesTimeSource() {}

base::TimeTicks TestCountUsesTimeSource::NowTicks() {
  now_calls_count_++;
  // Don't return 0, as it triggers some assertions.
  return base::TimeTicks() + base::TimeDelta::FromSeconds(1);
}

}  // namespace scheduler
}  // namespace blink
