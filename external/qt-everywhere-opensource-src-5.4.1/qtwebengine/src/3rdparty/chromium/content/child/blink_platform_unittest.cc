// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/child/blink_platform_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Derives BlinkPlatformImpl for testing shared timers.
class TestBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  TestBlinkPlatformImpl() : mock_monotonically_increasing_time_(0) {}

  // Returns mock time when enabled.
  virtual double monotonicallyIncreasingTime() OVERRIDE {
    if (mock_monotonically_increasing_time_ > 0.0)
      return mock_monotonically_increasing_time_;
    return BlinkPlatformImpl::monotonicallyIncreasingTime();
  }

  virtual void OnStartSharedTimer(base::TimeDelta delay) OVERRIDE {
    shared_timer_delay_ = delay;
  }

  base::TimeDelta shared_timer_delay() { return shared_timer_delay_; }

  void set_mock_monotonically_increasing_time(double mock_time) {
    mock_monotonically_increasing_time_ = mock_time;
  }

 private:
  base::TimeDelta shared_timer_delay_;
  double mock_monotonically_increasing_time_;

  DISALLOW_COPY_AND_ASSIGN(TestBlinkPlatformImpl);
};

TEST(BlinkPlatformTest, SuspendResumeSharedTimer) {
  base::MessageLoop message_loop;

  TestBlinkPlatformImpl platform_impl;

  // Set a timer to fire as soon as possible.
  platform_impl.setSharedTimerFireInterval(0);

  // Suspend timers immediately so the above timer wouldn't be fired.
  platform_impl.SuspendSharedTimer();

  // The above timer would have posted a task which can be processed out of the
  // message loop.
  base::RunLoop().RunUntilIdle();

  // Set a mock time after 1 second to simulate timers suspended for 1 second.
  double new_time = base::Time::Now().ToDoubleT() + 1;
  platform_impl.set_mock_monotonically_increasing_time(new_time);

  // Resume timers so that the timer set above will be set again to fire
  // immediately.
  platform_impl.ResumeSharedTimer();

  EXPECT_TRUE(base::TimeDelta() == platform_impl.shared_timer_delay());
}

}  // namespace content
