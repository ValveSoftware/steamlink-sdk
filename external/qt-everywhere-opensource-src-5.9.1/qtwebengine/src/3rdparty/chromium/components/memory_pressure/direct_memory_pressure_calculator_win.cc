// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/direct_memory_pressure_calculator_win.h"

#include "base/logging.h"
#include "base/process/process_metrics.h"

namespace memory_pressure {

#if defined(MEMORY_PRESSURE_IS_POLLING)

namespace {

const int kKBperMB = 1024;

}  // namespace

// A system is considered 'high memory' if it has more than 1.5GB of system
// memory available for use by the memory manager (not reserved for hardware
// and drivers). This is a fuzzy version of the ~2GB discussed below.
const int DirectMemoryPressureCalculator::kLargeMemoryThresholdMb = 1536;

// These are the default thresholds used for systems with < ~2GB of physical
// memory. Such systems have been observed to always maintain ~100MB of
// available memory, paging until that is the case. To try to avoid paging a
// threshold slightly above this is chosen. The moderate threshold is slightly
// less grounded in reality and chosen as 2.5x critical.
const int
    DirectMemoryPressureCalculator::kSmallMemoryDefaultModerateThresholdMb =
        500;
const int
    DirectMemoryPressureCalculator::kSmallMemoryDefaultCriticalThresholdMb =
        200;

// These are the default thresholds used for systems with >= ~2GB of physical
// memory. Such systems have been observed to always maintain ~300MB of
// available memory, paging until that is the case.
const int
    DirectMemoryPressureCalculator::kLargeMemoryDefaultModerateThresholdMb =
        1000;
const int
    DirectMemoryPressureCalculator::kLargeMemoryDefaultCriticalThresholdMb =
        400;

DirectMemoryPressureCalculator::DirectMemoryPressureCalculator()
    : moderate_threshold_mb_(0), critical_threshold_mb_(0) {
  InferThresholds();
}

DirectMemoryPressureCalculator::DirectMemoryPressureCalculator(
    int moderate_threshold_mb,
    int critical_threshold_mb)
    : moderate_threshold_mb_(moderate_threshold_mb),
      critical_threshold_mb_(critical_threshold_mb) {
  DCHECK_GE(moderate_threshold_mb_, critical_threshold_mb_);
  DCHECK_LE(0, critical_threshold_mb_);
}

DirectMemoryPressureCalculator::MemoryPressureLevel
DirectMemoryPressureCalculator::CalculateCurrentPressureLevel() {
  base::SystemMemoryInfoKB mem_info = {};
  if (!GetSystemMemoryInfo(&mem_info))
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;

  // How much system memory is actively available for use right now, in MBs.
  int phys_free = mem_info.free / kKBperMB;

  // TODO(chrisha): This should eventually care about address space pressure,
  // but the browser process (where this is running) effectively never runs out
  // of address space. Renderers occasionally do, but it does them no good to
  // have the browser process monitor address space pressure. Long term,
  // renderers should run their own address space pressure monitors and act
  // accordingly, with the browser making cross-process decisions based on
  // system memory pressure.

  // Determine if the physical memory is under critical memory pressure.
  if (phys_free <= critical_threshold_mb_)
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;

  // Determine if the physical memory is under moderate memory pressure.
  if (phys_free <= moderate_threshold_mb_)
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;

  // No memory pressure was detected.
  return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

bool DirectMemoryPressureCalculator::GetSystemMemoryInfo(
    base::SystemMemoryInfoKB* mem_info) const {
  return base::GetSystemMemoryInfo(mem_info);
}

void DirectMemoryPressureCalculator::InferThresholds() {
  // Determine if the memory installed is 'large' or 'small'. Default to 'large'
  // on failure, which uses more conservative thresholds.
  bool large_memory = true;
  base::SystemMemoryInfoKB mem_info = {};
  if (GetSystemMemoryInfo(&mem_info)) {
    large_memory = mem_info.total / kKBperMB >=
                   DirectMemoryPressureCalculator::kLargeMemoryThresholdMb;
  }

  if (large_memory) {
    moderate_threshold_mb_ = kLargeMemoryDefaultModerateThresholdMb;
    critical_threshold_mb_ = kLargeMemoryDefaultCriticalThresholdMb;
  } else {
    moderate_threshold_mb_ = kSmallMemoryDefaultModerateThresholdMb;
    critical_threshold_mb_ = kSmallMemoryDefaultCriticalThresholdMb;
  }
}

#endif  // defined(MEMORY_PRESSURE_IS_POLLING)

}  // namespace memory_pressure
