// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/memory_pressure_stats_collector.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/time/tick_clock.h"

namespace memory_pressure {

namespace {

using MemoryPressureLevel = MemoryPressureListener::MemoryPressureLevel;

// Converts a memory pressure level to an UMA enumeration value.
MemoryPressureLevelUMA MemoryPressureLevelToUmaEnumValue(
    MemoryPressureLevel level) {
  switch (level) {
    case MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      return UMA_MEMORY_PRESSURE_LEVEL_NONE;
    case MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      return UMA_MEMORY_PRESSURE_LEVEL_MODERATE;
    case MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      return UMA_MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  NOTREACHED();
  return UMA_MEMORY_PRESSURE_LEVEL_NONE;
}

// Converts an UMA enumeration value to a memory pressure level.
MemoryPressureLevel MemoryPressureLevelFromUmaEnumValue(
    MemoryPressureLevelUMA level) {
  switch (level) {
    case UMA_MEMORY_PRESSURE_LEVEL_NONE:
      return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
    case UMA_MEMORY_PRESSURE_LEVEL_MODERATE:
      return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
    case UMA_MEMORY_PRESSURE_LEVEL_CRITICAL:
      return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
    case UMA_MEMORY_PRESSURE_LEVEL_COUNT:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

// Converts a pressure state change to an UMA enumeration value.
MemoryPressureLevelChangeUMA MemoryPressureLevelChangeToUmaEnumValue(
    MemoryPressureLevel old_level,
    MemoryPressureLevel new_level) {
  switch (old_level) {
    case MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE: {
      if (new_level == MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE)
        return UMA_MEMORY_PRESSURE_LEVEL_CHANGE_NONE_TO_MODERATE;
      if (new_level == MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL)
        return UMA_MEMORY_PRESSURE_LEVEL_CHANGE_NONE_TO_CRITICAL;
      break;  // Should never happen; handled by the NOTREACHED below.
    }
    case MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE: {
      if (new_level == MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE)
        return UMA_MEMORY_PRESSURE_LEVEL_CHANGE_MODERATE_TO_NONE;
      if (new_level == MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL)
        return UMA_MEMORY_PRESSURE_LEVEL_CHANGE_MODERATE_TO_CRITICAL;
      break;  // Should never happen; handled by the NOTREACHED below.
    }
    case MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL: {
      if (new_level == MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE)
        return UMA_MEMORY_PRESSURE_LEVEL_CHANGE_CRITICAL_TO_MODERATE;
      if (new_level == MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE)
        return UMA_MEMORY_PRESSURE_LEVEL_CHANGE_CRITICAL_TO_MODERATE;
      break;  // Should never happen; handled by the NOTREACHED below.
    }
  }
  NOTREACHED();
  return UMA_MEMORY_PRESSURE_LEVEL_CHANGE_NONE_TO_MODERATE;
}

}  // namespace

MemoryPressureStatsCollector::MemoryPressureStatsCollector(
    base::TickClock* tick_clock)
    : tick_clock_(tick_clock),
      last_pressure_level_(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
}

void MemoryPressureStatsCollector::UpdateStatistics(
    MemoryPressureLevel current_pressure_level) {
  base::TimeTicks now = tick_clock_->NowTicks();

  // Special case: first call to the collector. Observations have just started
  // so there's nothing to report.
  if (last_update_time_.is_null()) {
    last_pressure_level_ = current_pressure_level;
    last_update_time_ = now;
    return;
  }

  // If the pressure level has transitioned then report this.
  if (last_pressure_level_ != current_pressure_level)
    ReportLevelChange(last_pressure_level_, current_pressure_level);

  // Increment the appropriate cumulative bucket.
  int index = MemoryPressureLevelToUmaEnumValue(current_pressure_level);
  unreported_cumulative_time_[index] += now - last_update_time_;

  // Update last pressure related state.
  last_pressure_level_ = current_pressure_level;
  last_update_time_ = now;

  // Make reports about the amount of time spent cumulatively at each level.
  for (size_t i = 0; i < arraysize(unreported_cumulative_time_); ++i) {
    // Report the largest number of whole seconds possible at this moment and
    // carry around the rest for a future report.
    if (unreported_cumulative_time_[i].is_zero())
      continue;
    int64_t seconds = unreported_cumulative_time_[i].InSeconds();
    if (seconds == 0)
      continue;
    unreported_cumulative_time_[i] -= base::TimeDelta::FromSeconds(seconds);

    ReportCumulativeTime(MemoryPressureLevelFromUmaEnumValue(
                             static_cast<MemoryPressureLevelUMA>(i)),
                         static_cast<int>(seconds));
  }
}

// static
void MemoryPressureStatsCollector::ReportCumulativeTime(
    MemoryPressureLevel pressure_level,
    int seconds) {
  // Use the more primitive STATIC_HISTOGRAM_POINTER_BLOCK macro because the
  // simple UMA_HISTOGRAM macros don't expose 'AddCount' functionality.
  STATIC_HISTOGRAM_POINTER_BLOCK(
      "Memory.PressureLevel",
      AddCount(MemoryPressureLevelToUmaEnumValue(pressure_level), seconds),
      base::LinearHistogram::FactoryGet(
          "Memory.PressureLevel", 1, UMA_MEMORY_PRESSURE_LEVEL_COUNT,
          UMA_MEMORY_PRESSURE_LEVEL_COUNT + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}

// static
void MemoryPressureStatsCollector::ReportLevelChange(
    MemoryPressureLevel old_pressure_level,
    MemoryPressureLevel new_pressure_level) {
  UMA_HISTOGRAM_ENUMERATION("Memory.PressureLevelChange",
                            MemoryPressureLevelChangeToUmaEnumValue(
                                old_pressure_level, new_pressure_level),
                            UMA_MEMORY_PRESSURE_LEVEL_CHANGE_COUNT);
}

}  // namespace memory_pressure
