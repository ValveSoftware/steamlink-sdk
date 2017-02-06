// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/memory_pressure_stats_collector.h"

#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

namespace {

// Histogram names.
const char kPressureLevel[] = "Memory.PressureLevel";
const char kPressureLevelChange[] = "Memory.PressureLevelChange";

}  // namespace

// Test version of the stats collector with a few extra accessors.
class TestMemoryPressureStatsCollector : public MemoryPressureStatsCollector {
 public:
  TestMemoryPressureStatsCollector(base::TickClock* tick_clock)
      : MemoryPressureStatsCollector(tick_clock) {}

  // Accessors.
  base::TimeDelta unreported_cumulative_time(int i) const {
    return unreported_cumulative_time_[i];
  }
  MemoryPressureLevel last_pressure_level() const {
    return last_pressure_level_;
  }
  base::TimeTicks last_update_time() const { return last_update_time_; }
};

// Test fixture.
class MemoryPressureStatsCollectorTest : public testing::Test {
 public:
  MemoryPressureStatsCollectorTest() : collector_(&tick_clock_) {}

  void Tick(int ms) {
    tick_clock_.Advance(base::TimeDelta::FromMilliseconds(ms));
  }

  // Validates expectations on the amount of accumulated (and unreported)
  // time (milliseconds) per pressure level.
  void ExpectAccumulated(int none_ms, int moderate_ms, int critical_ms) {
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(none_ms),
              collector_.unreported_cumulative_time(0));  // None.
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(moderate_ms),
              collector_.unreported_cumulative_time(1));  // Moderate.
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(critical_ms),
              collector_.unreported_cumulative_time(2));  // Critical.
  }

  // Validates expectations on the amount of reported time (seconds) per
  // pressure level.
  void ExpectReported(int none_s, int moderate_s, int critical_s) {
    int total_s = none_s + moderate_s + critical_s;

    // If the histogram should be empty then simply confirm that it doesn't
    // yet exist.
    if (total_s == 0) {
      EXPECT_TRUE(histograms_.GetTotalCountsForPrefix(kPressureLevel).empty());
      return;
    }

    histograms_.ExpectBucketCount(kPressureLevel, 0, none_s);      // None.
    histograms_.ExpectBucketCount(kPressureLevel, 1, moderate_s);  // Moderate.
    histograms_.ExpectBucketCount(kPressureLevel, 2, critical_s);  // Critical.
    histograms_.ExpectTotalCount(kPressureLevel, total_s);
  }

  base::SimpleTestTickClock tick_clock_;
  TestMemoryPressureStatsCollector collector_;
  base::HistogramTester histograms_;
};

TEST_F(MemoryPressureStatsCollectorTest, EndToEnd) {
  // Upon construction no statistics should yet have been reported.
  ExpectAccumulated(0, 0, 0);
  ExpectReported(0, 0, 0);
  histograms_.ExpectTotalCount(kPressureLevelChange, 0);

  // A first call should not invoke any reporting functions, but it should
  // modify member variables.
  Tick(500);
  collector_.UpdateStatistics(
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            collector_.last_pressure_level());
  EXPECT_EQ(tick_clock_.NowTicks(), collector_.last_update_time());
  ExpectAccumulated(0, 0, 0);
  ExpectReported(0, 0, 0);
  histograms_.ExpectTotalCount(kPressureLevelChange, 0);

  // A subsequent call with the same pressure level should increment the
  // cumulative time but not make a report, as less than one second of time
  // has been accumulated.
  Tick(500);
  collector_.UpdateStatistics(
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            collector_.last_pressure_level());
  EXPECT_EQ(tick_clock_.NowTicks(), collector_.last_update_time());
  ExpectAccumulated(500, 0, 0);
  ExpectReported(0, 0, 0);
  histograms_.ExpectTotalCount(kPressureLevelChange, 0);

  // Yet another call and this time a report should be made, as one second
  // of time has been accumulated. 500ms should remain unreported.
  Tick(1000);
  collector_.UpdateStatistics(
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            collector_.last_pressure_level());
  EXPECT_EQ(tick_clock_.NowTicks(), collector_.last_update_time());
  ExpectAccumulated(500, 0, 0);
  ExpectReported(1, 0, 0);
  histograms_.ExpectTotalCount(kPressureLevelChange, 0);

  // A subsequent call with a different pressure level should increment the
  // cumulative time and make several reports.
  Tick(2250);
  collector_.UpdateStatistics(
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            collector_.last_pressure_level());
  EXPECT_EQ(tick_clock_.NowTicks(), collector_.last_update_time());
  ExpectAccumulated(500, 250, 0);
  ExpectReported(1, 2, 0);
  histograms_.ExpectBucketCount(
      kPressureLevelChange, UMA_MEMORY_PRESSURE_LEVEL_CHANGE_NONE_TO_MODERATE,
      1);
  histograms_.ExpectTotalCount(kPressureLevelChange, 1);
}

}  // namespace memory_pressure
