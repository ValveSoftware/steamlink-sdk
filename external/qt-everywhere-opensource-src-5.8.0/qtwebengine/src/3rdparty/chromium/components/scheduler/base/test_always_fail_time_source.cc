// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/test_always_fail_time_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduler {

TestAlwaysFailTimeSource::TestAlwaysFailTimeSource() {
}

TestAlwaysFailTimeSource::~TestAlwaysFailTimeSource() {
}

base::TimeTicks TestAlwaysFailTimeSource::NowTicks() {
  ADD_FAILURE() << "NowTicks() was called!";
  return base::TimeTicks();
}

}  // namespace scheduler
