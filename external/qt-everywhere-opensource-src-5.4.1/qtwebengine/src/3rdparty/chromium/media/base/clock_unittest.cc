// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/clock.h"
#include "media/base/clock.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

namespace base {

// Provide a stream output operator so we can use EXPECT_EQ(...) with TimeDelta.
//
// TODO(scherkus): move this into the testing package.
static std::ostream& operator<<(std::ostream& stream, const TimeDelta& time) {
  return (stream << time.ToInternalValue());
}

}  // namespace

namespace media {

static const int kDurationInSeconds = 120;

class ClockTest : public ::testing::Test {
 public:
  ClockTest() : clock_(&test_tick_clock_) {
    SetDuration();
  }

 protected:
  void SetDuration() {
    const base::TimeDelta kDuration =
        base::TimeDelta::FromSeconds(kDurationInSeconds);
    clock_.SetDuration(kDuration);
    EXPECT_EQ(kDuration, clock_.Duration());
  }

  void AdvanceSystemTime(base::TimeDelta delta) {
    test_tick_clock_.Advance(delta);
  }

  base::SimpleTestTickClock test_tick_clock_;
  Clock clock_;
  base::TimeDelta time_elapsed_;
};

TEST_F(ClockTest, Created) {
  const base::TimeDelta kExpected = base::TimeDelta::FromSeconds(0);
  EXPECT_EQ(kExpected, clock_.Elapsed());
}

TEST_F(ClockTest, Play_NormalSpeed) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimeToAdvance = base::TimeDelta::FromSeconds(2);

  EXPECT_EQ(kZero, clock_.Play());
  AdvanceSystemTime(kTimeToAdvance);
  EXPECT_EQ(kTimeToAdvance, clock_.Elapsed());
}

TEST_F(ClockTest, Play_DoubleSpeed) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimeToAdvance = base::TimeDelta::FromSeconds(5);

  clock_.SetPlaybackRate(2.0f);
  EXPECT_EQ(kZero, clock_.Play());
  AdvanceSystemTime(kTimeToAdvance);
  EXPECT_EQ(2 * kTimeToAdvance, clock_.Elapsed());
}

TEST_F(ClockTest, Play_HalfSpeed) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimeToAdvance = base::TimeDelta::FromSeconds(4);

  clock_.SetPlaybackRate(0.5f);
  EXPECT_EQ(kZero, clock_.Play());
  AdvanceSystemTime(kTimeToAdvance);
  EXPECT_EQ(kTimeToAdvance / 2, clock_.Elapsed());
}

TEST_F(ClockTest, Play_ZeroSpeed) {
  // We'll play for 2 seconds at normal speed, 4 seconds at zero speed, and 8
  // seconds at normal speed.
  const base::TimeDelta kZero;
  const base::TimeDelta kPlayDuration1 = base::TimeDelta::FromSeconds(2);
  const base::TimeDelta kPlayDuration2 = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kPlayDuration3 = base::TimeDelta::FromSeconds(8);
  const base::TimeDelta kExpected = kPlayDuration1 + kPlayDuration3;

  EXPECT_EQ(kZero, clock_.Play());

  AdvanceSystemTime(kPlayDuration1);
  clock_.SetPlaybackRate(0.0f);
  AdvanceSystemTime(kPlayDuration2);
  clock_.SetPlaybackRate(1.0f);
  AdvanceSystemTime(kPlayDuration3);

  EXPECT_EQ(kExpected, clock_.Elapsed());
}

TEST_F(ClockTest, Play_MultiSpeed) {
  // We'll play for 2 seconds at half speed, 4 seconds at normal speed, and 8
  // seconds at double speed.
  const base::TimeDelta kZero;
  const base::TimeDelta kPlayDuration1 = base::TimeDelta::FromSeconds(2);
  const base::TimeDelta kPlayDuration2 = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kPlayDuration3 = base::TimeDelta::FromSeconds(8);
  const base::TimeDelta kExpected =
      kPlayDuration1 / 2 + kPlayDuration2 + 2 * kPlayDuration3;

  clock_.SetPlaybackRate(0.5f);
  EXPECT_EQ(kZero, clock_.Play());
  AdvanceSystemTime(kPlayDuration1);

  clock_.SetPlaybackRate(1.0f);
  AdvanceSystemTime(kPlayDuration2);

  clock_.SetPlaybackRate(2.0f);
  AdvanceSystemTime(kPlayDuration3);
  EXPECT_EQ(kExpected, clock_.Elapsed());
}

TEST_F(ClockTest, Pause) {
  const base::TimeDelta kZero;
  const base::TimeDelta kPlayDuration = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kPauseDuration = base::TimeDelta::FromSeconds(20);
  const base::TimeDelta kExpectedFirstPause = kPlayDuration;
  const base::TimeDelta kExpectedSecondPause = 2 * kPlayDuration;

  // Play for 4 seconds.
  EXPECT_EQ(kZero, clock_.Play());
  AdvanceSystemTime(kPlayDuration);

  // Pause for 20 seconds.
  EXPECT_EQ(kExpectedFirstPause, clock_.Pause());
  EXPECT_EQ(kExpectedFirstPause, clock_.Elapsed());
  AdvanceSystemTime(kPauseDuration);
  EXPECT_EQ(kExpectedFirstPause, clock_.Elapsed());

  // Play again for 4 more seconds.
  EXPECT_EQ(kExpectedFirstPause, clock_.Play());
  AdvanceSystemTime(kPlayDuration);
  EXPECT_EQ(kExpectedSecondPause, clock_.Pause());
  EXPECT_EQ(kExpectedSecondPause, clock_.Elapsed());
}

TEST_F(ClockTest, SetTime_Paused) {
  const base::TimeDelta kFirstTime = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kSecondTime = base::TimeDelta::FromSeconds(16);

  clock_.SetTime(kFirstTime, clock_.Duration());
  EXPECT_EQ(kFirstTime, clock_.Elapsed());
  clock_.SetTime(kSecondTime, clock_.Duration());
  EXPECT_EQ(kSecondTime, clock_.Elapsed());
}

TEST_F(ClockTest, SetTime_Playing) {
  // We'll play for 4 seconds, then set the time to 12, then play for 4 more
  // seconds.
  const base::TimeDelta kZero;
  const base::TimeDelta kPlayDuration = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kUpdatedTime = base::TimeDelta::FromSeconds(12);
  const base::TimeDelta kExpected = kUpdatedTime + kPlayDuration;

  EXPECT_EQ(kZero, clock_.Play());
  AdvanceSystemTime(kPlayDuration);

  clock_.SetTime(kUpdatedTime, clock_.Duration());
  AdvanceSystemTime(kPlayDuration);
  EXPECT_EQ(kExpected, clock_.Elapsed());
}

TEST_F(ClockTest, CapAtMediaDuration_Paused) {
  const base::TimeDelta kDuration =
      base::TimeDelta::FromSeconds(kDurationInSeconds);
  const base::TimeDelta kTimeOverDuration =
      base::TimeDelta::FromSeconds(kDurationInSeconds + 4);

  // Elapsed time should always be capped at the duration of the media.
  clock_.SetTime(kTimeOverDuration, kTimeOverDuration);
  EXPECT_EQ(kDuration, clock_.Elapsed());
}

TEST_F(ClockTest, CapAtMediaDuration_Playing) {
  const base::TimeDelta kZero;
  const base::TimeDelta kDuration =
      base::TimeDelta::FromSeconds(kDurationInSeconds);
  const base::TimeDelta kTimeOverDuration =
      base::TimeDelta::FromSeconds(kDurationInSeconds + 4);

  // Play for twice as long as the duration of the media.
  EXPECT_EQ(kZero, clock_.Play());
  AdvanceSystemTime(2 * kDuration);
  EXPECT_EQ(kDuration, clock_.Elapsed());

  // Manually set the time past the duration.
  clock_.SetTime(kTimeOverDuration, kTimeOverDuration);
  EXPECT_EQ(kDuration, clock_.Elapsed());
}

TEST_F(ClockTest, SetMaxTime) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimeInterval = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kMaxTime = base::TimeDelta::FromSeconds(6);

  EXPECT_EQ(kZero, clock_.Play());
  clock_.SetMaxTime(kMaxTime);
  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kTimeInterval, clock_.Elapsed());

  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime, clock_.Elapsed());

  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime, clock_.Elapsed());
}

TEST_F(ClockTest, SetMaxTime_MultipleTimes) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimeInterval = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kMaxTime1 = base::TimeDelta::FromSeconds(6);
  const base::TimeDelta kMaxTime2 = base::TimeDelta::FromSeconds(12);

  EXPECT_EQ(kZero, clock_.Play());
  clock_.SetMaxTime(clock_.Duration());
  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kTimeInterval, clock_.Elapsed());

  clock_.SetMaxTime(kMaxTime1);
  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime1, clock_.Elapsed());

  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime1, clock_.Elapsed());

  clock_.SetMaxTime(kMaxTime2);
  EXPECT_EQ(kMaxTime1, clock_.Elapsed());

  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime1 + kTimeInterval, clock_.Elapsed());

  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime2, clock_.Elapsed());
}

}  // namespace media
