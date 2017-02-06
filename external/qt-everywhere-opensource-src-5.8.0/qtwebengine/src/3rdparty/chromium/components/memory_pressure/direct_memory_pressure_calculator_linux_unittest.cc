// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/direct_memory_pressure_calculator_linux.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

namespace {

static const int kKBperMB = 1024;
static const int kTotalMemoryInMB = 4096;

}  // namespace

// This is out of the anonymous namespace space because it is a friend of
// DirectMemoryPressureCalculator.
class TestDirectMemoryPressureCalculator
    : public DirectMemoryPressureCalculator {
 public:
  explicit TestDirectMemoryPressureCalculator()
      : DirectMemoryPressureCalculator(20, 10) {
    // The values passed to the MemoryPressureCalculator constructor are dummy
    // values that are immediately overwritten by InferTresholds.

    // Simulate 4GB of RAM.
    mem_info_.total = kTotalMemoryInMB * kKBperMB;

    // Run InferThresholds using the test fixture's GetSystemMemoryStatus.
    InferThresholds();
  }

  TestDirectMemoryPressureCalculator(int total_memory_mb,
                                     int moderate_threshold_mb,
                                     int critical_threshold_mb)
      : DirectMemoryPressureCalculator(moderate_threshold_mb,
                                       critical_threshold_mb) {
    mem_info_.total = total_memory_mb * kKBperMB;
  }

  // Sets up the memory status to reflect the provided absolute memory left.
  void SetMemoryFree(int phys_left_mb) {
    // |total| is set in the constructor and not modified.

    // Set the amount of free memory.
    mem_info_.free = phys_left_mb * kKBperMB;
    DCHECK_LT(mem_info_.free, mem_info_.total);
  }

  void SetNone() { SetMemoryFree(moderate_threshold_mb() + 1); }

  void SetModerate() { SetMemoryFree(moderate_threshold_mb() - 1); }

  void SetCritical() { SetMemoryFree(critical_threshold_mb() - 1); }

 private:
  bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* mem_info) const override {
    // Simply copy the memory information set by the test fixture.
    *mem_info = mem_info_;
    return true;
  }

  base::SystemMemoryInfoKB mem_info_;
};

class DirectMemoryPressureCalculatorTest : public testing::Test {
 public:
  void CalculateCurrentPressureLevelTest(
      TestDirectMemoryPressureCalculator* calc) {
    int mod = calc->moderate_threshold_mb();
    calc->SetMemoryFree(mod + 1);
    EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
              calc->CalculateCurrentPressureLevel());

    calc->SetNone();
    EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
              calc->CalculateCurrentPressureLevel());

    calc->SetMemoryFree(mod);
    EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              calc->CalculateCurrentPressureLevel());

    calc->SetMemoryFree(mod - 1);
    EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              calc->CalculateCurrentPressureLevel());

    int crit = calc->critical_threshold_mb();
    calc->SetMemoryFree(crit + 1);
    EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              calc->CalculateCurrentPressureLevel());

    calc->SetModerate();
    EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              calc->CalculateCurrentPressureLevel());

    EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              calc->CalculateCurrentPressureLevel());

    calc->SetMemoryFree(crit);
    EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
              calc->CalculateCurrentPressureLevel());

    calc->SetMemoryFree(crit - 1);
    EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
              calc->CalculateCurrentPressureLevel());

    calc->SetCritical();
    EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
              calc->CalculateCurrentPressureLevel());
  }
};

// Tests the fundamental direct calculation of memory pressure with automatic
// small-memory thresholds.
TEST_F(DirectMemoryPressureCalculatorTest,
       CalculateCurrentMemoryPressureLevel) {
  TestDirectMemoryPressureCalculator calc;

  static const int kModerateMb =
      (100 - DirectMemoryPressureCalculator::kDefaultModerateThresholdPc) *
      kTotalMemoryInMB / 100;
  static const int kCriticalMb =
      (100 - DirectMemoryPressureCalculator::kDefaultCriticalThresholdPc) *
      kTotalMemoryInMB / 100;

  EXPECT_EQ(kModerateMb, calc.moderate_threshold_mb());
  EXPECT_EQ(kCriticalMb, calc.critical_threshold_mb());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentPressureLevelTest(&calc));
}

// Tests the fundamental direct calculation of memory pressure with manually
// specified threshold levels.
TEST_F(DirectMemoryPressureCalculatorTest,
       CalculateCurrentMemoryPressureLevelCustom) {
  static const int kSystemMb = 512;
  static const int kModerateMb = 256;
  static const int kCriticalMb = 128;

  TestDirectMemoryPressureCalculator calc(kSystemMb, kModerateMb, kCriticalMb);

  EXPECT_EQ(kModerateMb, calc.moderate_threshold_mb());
  EXPECT_EQ(kCriticalMb, calc.critical_threshold_mb());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentPressureLevelTest(&calc));
}

}  // namespace memory_pressure
