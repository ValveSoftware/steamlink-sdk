// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/direct_memory_pressure_calculator_linux.h"

#include "base/files/file_util.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/threading/thread_restrictions.h"

namespace memory_pressure {

namespace {

const int kKiBperMiB = 1024;

// Used to calculate a moving average of faults/sec.  Our sample times are
// inconsistent because MemoryPressureMonitor calls
// CalculateCurrentPressureLevel more frequently when memory pressure is high,
// and because we're measuring CPU time instead of real time.  So our
// exponentially weighted moving average is generalized to a low-pass filter.
//
// Represents the amount of CPU time, in seconds, for a sample to be 50%
// forgotten in our moving average.  Systems with more CPUs that are under high
// load will have a moving average that changes more quickly than a system with
// fewer CPUs under the same load.  Do not normalize based on the number of CPUs
// because this behavior is accurate:  if a system with a large number of CPUs
// is getting lots of work done without page faulting, it must not be under
// memory pressure.
//
// TODO(thomasanderson): Experimentally determine the correct value for this
// constant.
const double kLowPassHalfLife = 30.0;

// Returns the amount of memory that is available for use right now, or that can
// be easily reclaimed by the OS, in MBs.
int GetAvailableSystemMemoryMiB(const base::SystemMemoryInfoKB* mem_info) {
  return mem_info->available
             ? mem_info->available / kKiBperMiB
             : (mem_info->free + mem_info->buffers + mem_info->cached) /
                   kKiBperMiB;
}

}  // namespace

// Thresholds at which we consider the system being under moderate/critical
// memory pressure. They represent the percentage of system memory in use.
const int DirectMemoryPressureCalculator::kDefaultModerateThresholdPc = 70;
const int DirectMemoryPressureCalculator::kDefaultCriticalThresholdPc = 90;

DirectMemoryPressureCalculator::DirectMemoryPressureCalculator()
    : moderate_threshold_mb_(0),
      critical_threshold_mb_(0) {
  InferThresholds();
  InitPageFaultMonitor();
}

DirectMemoryPressureCalculator::DirectMemoryPressureCalculator(
    int moderate_threshold_mb,
    int critical_threshold_mb)
    : moderate_threshold_mb_(moderate_threshold_mb),
      critical_threshold_mb_(critical_threshold_mb) {
  DCHECK_GE(moderate_threshold_mb_, critical_threshold_mb_);
  DCHECK_LE(0, critical_threshold_mb_);
  InitPageFaultMonitor();
}

DirectMemoryPressureCalculator::MemoryPressureLevel
DirectMemoryPressureCalculator::PressureCausedByThrashing(
    const base::SystemMemoryInfoKB& mem_info) {
  base::TimeDelta new_user_exec_time = GetUserCpuTimeSinceBoot();
  if (new_user_exec_time == base::TimeDelta())
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  uint64_t new_major_page_faults = mem_info.pgmajfault;
  if (new_user_exec_time != base::TimeDelta() && new_major_page_faults &&
      (new_user_exec_time - last_user_exec_time_) != base::TimeDelta()) {
    double delta_user_exec_time =
        (new_user_exec_time - last_user_exec_time_).InSecondsF();
    double delta_major_page_faults =
        new_major_page_faults - last_major_page_faults_;

    double sampled_faults_per_second =
        delta_major_page_faults / delta_user_exec_time;

    double adjusted_ewma_coefficient =
        1 - exp2(-delta_user_exec_time / low_pass_half_life_seconds_);

    current_faults_per_second_ =
        adjusted_ewma_coefficient * sampled_faults_per_second +
        (1 - adjusted_ewma_coefficient) * current_faults_per_second_;

    last_user_exec_time_ = new_user_exec_time;
    last_major_page_faults_ = new_major_page_faults;
  }

  if (current_faults_per_second_ >
      critical_multiplier_ * AverageFaultsPerSecond()) {
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  if (current_faults_per_second_ >
      moderate_multiplier_ * AverageFaultsPerSecond()) {
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  }
  return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

DirectMemoryPressureCalculator::MemoryPressureLevel
DirectMemoryPressureCalculator::PressureCausedByOOM(
    const base::SystemMemoryInfoKB& mem_info) {
  int phys_free = GetAvailableSystemMemoryMiB(&mem_info);

  if (phys_free <= critical_threshold_mb_)
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  if (phys_free <= moderate_threshold_mb_)
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

DirectMemoryPressureCalculator::MemoryPressureLevel
DirectMemoryPressureCalculator::CalculateCurrentPressureLevel() {
  base::SystemMemoryInfoKB mem_info = {};
  if (!GetSystemMemoryInfo(&mem_info))
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;

  return std::max(PressureCausedByThrashing(mem_info),
                  PressureCausedByOOM(mem_info));
}

bool DirectMemoryPressureCalculator::GetSystemMemoryInfo(
    base::SystemMemoryInfoKB* mem_info) const {
  return base::GetSystemMemoryInfo(mem_info);
}

base::TimeDelta DirectMemoryPressureCalculator::GetUserCpuTimeSinceBoot()
    const {
  return base::GetUserCpuTimeSinceBoot();
}

void DirectMemoryPressureCalculator::InferThresholds() {
  base::SystemMemoryInfoKB mem_info = {};
  if (!GetSystemMemoryInfo(&mem_info))
    return;

  moderate_threshold_mb_ =
      mem_info.total * (100 - kDefaultModerateThresholdPc) / 100 / kKiBperMiB;
  critical_threshold_mb_ =
      mem_info.total * (100 - kDefaultCriticalThresholdPc) / 100 / kKiBperMiB;
}

void DirectMemoryPressureCalculator::InitPageFaultMonitor() {
  low_pass_half_life_seconds_ = kLowPassHalfLife;
  last_user_exec_time_ = GetUserCpuTimeSinceBoot();
  base::SystemMemoryInfoKB mem_info = {};
  last_major_page_faults_ =
      GetSystemMemoryInfo(&mem_info) ? mem_info.pgmajfault : 0;
  current_faults_per_second_ = AverageFaultsPerSecond();
}

double DirectMemoryPressureCalculator::AverageFaultsPerSecond() const {
  return last_major_page_faults_ == 0
             ? last_user_exec_time_.InSecondsF() / last_major_page_faults_
             : 0;
}

}  // namespace memory_pressure
