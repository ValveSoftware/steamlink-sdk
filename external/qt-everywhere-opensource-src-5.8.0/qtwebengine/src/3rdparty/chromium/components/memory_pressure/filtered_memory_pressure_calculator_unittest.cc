// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/filtered_memory_pressure_calculator.h"

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/memory_pressure/test_memory_pressure_calculator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

#if defined(MEMORY_PRESSURE_IS_POLLING)

class FilteredMemoryPressureCalculatorTest : public testing::Test {
 public:
  FilteredMemoryPressureCalculatorTest()
      : filter_(&calculator_, &tick_clock_) {}

  // Advances the tick clock by the given number of milliseconds.
  void Tick(int ms) {
    tick_clock_.Advance(base::TimeDelta::FromMilliseconds(ms));
  }

  // Delegates that are injected into the object under test.
  TestMemoryPressureCalculator calculator_;
  base::SimpleTestTickClock tick_clock_;

  // The object under test.
  FilteredMemoryPressureCalculator filter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FilteredMemoryPressureCalculatorTest);
};

TEST_F(FilteredMemoryPressureCalculatorTest, FirstCallInvokesCalculator) {
  calculator_.SetCritical();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(1, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());
}

TEST_F(FilteredMemoryPressureCalculatorTest, CooldownCriticalToModerate) {
  calculator_.SetCritical();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(1, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());

  // Initiate a cooldown period by stepping forward and taking another sample.
  Tick(100);
  calculator_.SetModerate();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(2, calculator_.calls());
  EXPECT_TRUE(filter_.cooldown_in_progress());
  EXPECT_EQ(tick_clock_.NowTicks(), filter_.cooldown_start_time());
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.cooldown_high_tide());

  // Take another sample and it should still not have changed.
  Tick(100);
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(3, calculator_.calls());
  EXPECT_TRUE(filter_.cooldown_in_progress());
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.cooldown_high_tide());

  // Step the cooldown period and it should change state.
  Tick(FilteredMemoryPressureCalculator::kCriticalPressureCooldownPeriodMs);
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(4, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());
}

TEST_F(FilteredMemoryPressureCalculatorTest,
       CooldownCriticalToModerateViaNone) {
  calculator_.SetCritical();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(1, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());

  // Initiate a cooldown period by stepping forward and taking another sample.
  // First go directly to no memory pressure before passing back through
  // moderate. The final result should be a moderate memory pressure.
  Tick(100);
  calculator_.SetNone();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(2, calculator_.calls());
  EXPECT_TRUE(filter_.cooldown_in_progress());
  EXPECT_EQ(tick_clock_.NowTicks(), filter_.cooldown_start_time());
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            filter_.cooldown_high_tide());

  // Take another sample and it should still not have changed.
  Tick(100);
  calculator_.SetModerate();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(3, calculator_.calls());
  EXPECT_TRUE(filter_.cooldown_in_progress());
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.cooldown_high_tide());

  // Step the cooldown period and it should change state.
  Tick(FilteredMemoryPressureCalculator::kCriticalPressureCooldownPeriodMs);
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(4, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());
}

TEST_F(FilteredMemoryPressureCalculatorTest, CooldownCriticalToNone) {
  calculator_.SetCritical();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(1, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());

  // Initiate a cooldown period by stepping forward and taking another sample.
  Tick(100);
  calculator_.SetNone();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(2, calculator_.calls());
  EXPECT_TRUE(filter_.cooldown_in_progress());
  EXPECT_EQ(tick_clock_.NowTicks(), filter_.cooldown_start_time());
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            filter_.cooldown_high_tide());

  // Take another sample and it should still not have changed.
  Tick(100);
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(3, calculator_.calls());
  EXPECT_TRUE(filter_.cooldown_in_progress());
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            filter_.cooldown_high_tide());

  // Step the cooldown period and it should change state.
  Tick(FilteredMemoryPressureCalculator::kModeratePressureCooldownPeriodMs);
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(4, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());
}

TEST_F(FilteredMemoryPressureCalculatorTest, CooldownModerateToNone) {
  calculator_.SetModerate();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(1, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());

  // Initiate a cooldown period by stepping forward and taking another sample.
  Tick(100);
  calculator_.SetNone();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(2, calculator_.calls());
  EXPECT_TRUE(filter_.cooldown_in_progress());
  EXPECT_EQ(tick_clock_.NowTicks(), filter_.cooldown_start_time());
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            filter_.cooldown_high_tide());

  // Take another sample and it should still not have changed.
  Tick(100);
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(3, calculator_.calls());
  EXPECT_TRUE(filter_.cooldown_in_progress());
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            filter_.cooldown_high_tide());

  // Step the cooldown period and it should change state.
  Tick(FilteredMemoryPressureCalculator::kModeratePressureCooldownPeriodMs);
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(4, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());
}

TEST_F(FilteredMemoryPressureCalculatorTest, InterruptedCooldownModerate) {
  calculator_.SetModerate();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(1, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());

  // Initiate a cooldown period by stepping forward and taking another sample.
  Tick(100);
  calculator_.SetNone();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(2, calculator_.calls());
  EXPECT_TRUE(filter_.cooldown_in_progress());
  EXPECT_EQ(tick_clock_.NowTicks(), filter_.cooldown_start_time());
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            filter_.cooldown_high_tide());

  // Take another sample and it should still not have changed. Since the
  // pressure level has increased back to moderate it should also end the
  // cooldown period.
  Tick(100);
  calculator_.SetModerate();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(3, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());
}

TEST_F(FilteredMemoryPressureCalculatorTest, InterruptedCooldownCritical) {
  calculator_.SetModerate();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(1, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());

  // Initiate a cooldown period by stepping forward and taking another sample.
  Tick(100);
  calculator_.SetNone();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(2, calculator_.calls());
  EXPECT_TRUE(filter_.cooldown_in_progress());
  EXPECT_EQ(tick_clock_.NowTicks(), filter_.cooldown_start_time());
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            filter_.cooldown_high_tide());

  // Take another sample and it should still not have changed. Since the
  // pressure level has increased to critical it ends the cooldown period and
  // immediately reports the critical memory pressure.
  Tick(100);
  calculator_.SetCritical();
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            filter_.CalculateCurrentPressureLevel());
  EXPECT_EQ(3, calculator_.calls());
  EXPECT_FALSE(filter_.cooldown_in_progress());
}

#endif  // defined(MEMORY_PRESSURE_IS_POLLING)

}  // namespace memory_pressure
