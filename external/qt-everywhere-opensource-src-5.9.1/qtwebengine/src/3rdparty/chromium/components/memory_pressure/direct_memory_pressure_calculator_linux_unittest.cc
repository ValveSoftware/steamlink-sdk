// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/direct_memory_pressure_calculator_linux.h"

#include "base/process/process_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

namespace {

const int kKBperMB = 1024;
const int kTotalMemoryInMB = 4096;

}  // namespace

// This is out of the anonymous namespace space because it is a friend of
// DirectMemoryPressureCalculator.
class TestDirectMemoryPressureCalculator
    : public DirectMemoryPressureCalculator {
 public:
  TestDirectMemoryPressureCalculator()
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
    mem_info_.pgmajfault = 0;
  }

  void InitPageFaultMonitor() override {}

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

  double GetEwma() { return current_faults_per_second_; }

  // Set the next CPU time to be read.
  void SetCpuTime(double cpu_time) {
    user_cpu_time_ = base::TimeDelta::FromSecondsD(cpu_time);
  }
  // Set the next page faults value to be read.
  void SetPageFaults(uint64_t page_faults) {
    mem_info_.pgmajfault = page_faults;
  }

  void SetPageFaultMonitorState(double user_exec_time,
                                uint64_t major_page_faults,
                                double current_faults,
                                double low_pass_half_life,
                                double moderate_multiplier,
                                double critical_multiplier) {
    last_user_exec_time_ = base::TimeDelta::FromSecondsD(user_exec_time);
    last_major_page_faults_ = major_page_faults;
    current_faults_per_second_ = current_faults;
    low_pass_half_life_seconds_ = low_pass_half_life;
    moderate_multiplier_ = moderate_multiplier;
    critical_multiplier_ = critical_multiplier;
  }

 private:
  bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* mem_info) const override {
    // Simply copy the memory information set by the test fixture.
    *mem_info = mem_info_;
    return true;
  }

  base::TimeDelta GetUserCpuTimeSinceBoot() const override {
    return user_cpu_time_;
  }

  base::TimeDelta user_cpu_time_;
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

// Double-check the math of the Ewma portion of the page fault monitor.
TEST_F(DirectMemoryPressureCalculatorTest, Ewma) {
  double half_life = 100.0;

  double cpu_time = 0;
  uint64_t page_faults = 1;

  double ewma = 100.0;

  TestDirectMemoryPressureCalculator calc;
  calc.SetPageFaultMonitorState(cpu_time, page_faults, ewma, half_life, 0, 0);
  // Advance by one half-life.  The ewma should be cut in half.
  calc.SetCpuTime(half_life);
  calc.SetPageFaults(page_faults);
  calc.CalculateCurrentPressureLevel();
  ewma /= 2;
  EXPECT_DOUBLE_EQ(ewma, calc.GetEwma());

  // We should get the same result if we advance by increments of 10 up to 100.
  ewma = 100.0;
  calc.SetPageFaultMonitorState(cpu_time, page_faults, ewma, half_life, 0, 0);
  static const int kIncrements = 10;
  for(int i = 1; i <= kIncrements; i++) {
    calc.SetCpuTime(i * half_life / kIncrements);
    calc.SetPageFaults(page_faults);
    calc.CalculateCurrentPressureLevel();
  }
  ewma /= 2;
  EXPECT_DOUBLE_EQ(ewma, calc.GetEwma());

  static const struct {
    uint64_t delta_time;
    uint64_t delta_faults;
    double expected_ewma;
  } kSamples[] = {
      // 0.5*0.0 + 0.5*10 = 5.0
      { 1, 10,  5.0        },
      // 0.5*5.0 + 0.5*10 = 7.5
      { 1, 10,  7.5        },
      // 0.25*7.5 + 0.75*(0/2) = 1.875
      { 2, 0,   1.875      },
      // 0.125*1.875 + 0.875*(420/3) = 122.734375
      { 3, 420, 122.734375 },
      // 0.5*122.734375 + 0.5*24 = 73.3671875
      { 1, 24,  73.3671875 },
  };

  cpu_time = 1;
  page_faults = 1;
  ewma = 0.0;
  half_life = 1.0;
  calc.SetPageFaultMonitorState(cpu_time, page_faults, ewma, half_life, 0, 0);
  for (size_t i = 0 ; i < arraysize(kSamples); i++) {
    cpu_time += kSamples[i].delta_time;
    page_faults += kSamples[i].delta_faults;
    calc.SetCpuTime(cpu_time);
    calc.SetPageFaults(page_faults);
    calc.CalculateCurrentPressureLevel();
    EXPECT_DOUBLE_EQ(kSamples[i].expected_ewma, calc.GetEwma());
  }
}

}  // namespace memory_pressure
