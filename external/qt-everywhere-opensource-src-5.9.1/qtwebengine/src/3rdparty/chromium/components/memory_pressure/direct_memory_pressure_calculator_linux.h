// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_DIRECT_MEMORY_PRESSURE_CALCULATOR_LINUX_H_
#define COMPONENTS_MEMORY_PRESSURE_DIRECT_MEMORY_PRESSURE_CALCULATOR_LINUX_H_

#include "base/macros.h"
#include "base/process/process_metrics.h"
#include "components/memory_pressure/memory_pressure_calculator.h"

namespace base {
class TimeDelta;
}

namespace memory_pressure {

// OS-specific implementation of MemoryPressureCalculator. This is only defined
// and used on platforms that do not have native memory pressure signals
// (ChromeOS, Linux, Windows). OSes that do have native signals simply hook into
// the appropriate subsystem (Android, Mac OS X).
class DirectMemoryPressureCalculator : public MemoryPressureCalculator {
 public:
  // Exposed for unittesting. See .cc file for detailed discussion of these
  // constants.
  static const int kDefaultModerateThresholdPc;
  static const int kDefaultCriticalThresholdPc;

  // Default constructor. Will choose thresholds automatically based on the
  // actual amount of system memory installed.
  DirectMemoryPressureCalculator();

  // Constructor with explicit memory thresholds. These represent the amount of
  // free memory below which the applicable memory pressure state applies.
  DirectMemoryPressureCalculator(int moderate_threshold_mb,
                                 int critical_threshold_mb);

  ~DirectMemoryPressureCalculator() override {}

  // Calculates the current pressure level.
  MemoryPressureLevel CalculateCurrentPressureLevel() override;

  int moderate_threshold_mb() const { return moderate_threshold_mb_; }
  int critical_threshold_mb() const { return critical_threshold_mb_; }

 private:
  friend class TestDirectMemoryPressureCalculator;

  MemoryPressureLevel PressureCausedByThrashing(
      const base::SystemMemoryInfoKB& mem_info);
  MemoryPressureLevel PressureCausedByOOM(
      const base::SystemMemoryInfoKB& mem_info);

  // Gets system memory status. This is virtual as a unittesting hook and by
  // default this invokes base::GetSystemMemoryInfo.
  virtual bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* mem_info) const;

  // We use CPU time instead of real time because this eliminates the variable
  // of average system load.  Consumer machines are idle most of the time, so if
  // we used real time, the average faults/sec would be very low.  As soon as
  // the user loads anything, the instantaneous faults/sec would increase, and
  // we would overreport the memory pressure.
  //
  // We also ignore system time because we don't want to include the time the OS
  // takes to swap pages in/out on behalf of processes.
  //
  // Virtual as a unittesting hook.
  virtual base::TimeDelta GetUserCpuTimeSinceBoot() const;

  // Uses GetSystemMemoryInfo to automatically infer appropriate values for
  // moderate_threshold_mb_ and critical_threshold_mb_.
  void InferThresholds();

  // Initialize the page fault monitor state so CalculateCurrentPressureLevel
  // will have a delta to analyze.
  virtual void InitPageFaultMonitor();

  // Computes last_major_page_faults_/last_user_exec_time_ with some
  // protection.
  double AverageFaultsPerSecond() const;

  // Threshold amounts of available memory that trigger pressure levels. See
  // memory_pressure_monitor_win.cc for a discussion of reasonable values for
  // these.
  int moderate_threshold_mb_;
  int critical_threshold_mb_;

  // State needed by the hard page fault monitor.  These values are assumed
  // not to overflow.
  base::TimeDelta last_user_exec_time_;
  uint64_t last_major_page_faults_;
  // An exponentially weighted moving average of the sampled faults/sec.
  double current_faults_per_second_;

  double low_pass_half_life_seconds_;

  // |current_faults_per_second_| must be at least |kModerateMultiplier| *
  // AverageFaultsPerSecond() to report moderate pressure, and similarly for
  // critical pressure.  Non-const members for unit testing.
  //
  // TODO(thomasanderson): Experimentally determine the correct value for these
  // constants.
  double moderate_multiplier_ = 5.0;
  double critical_multiplier_ = 10.0;

  DISALLOW_COPY_AND_ASSIGN(DirectMemoryPressureCalculator);
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_DIRECT_MEMORY_PRESSURE_CALCULATOR_LINUX_H_
