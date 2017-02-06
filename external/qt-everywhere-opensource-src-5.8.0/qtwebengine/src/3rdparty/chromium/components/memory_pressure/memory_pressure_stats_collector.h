// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_STATS_COLLECTOR_H_
#define COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_STATS_COLLECTOR_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/memory_pressure/memory_pressure_listener.h"

namespace base {
class TickClock;
}

namespace memory_pressure {

// Enumeration of UMA memory pressure levels. The pressure level enum defined in
// MemoryPressureListener is non-contiguous. This enum is a contiguous version
// of that for use with UMA. Both it and histograms.xml must be kept in sync
// with the MemoryPressureListener enum. Included in the header so that
// UMA_MEMORY_PRESSURE_LEVEL_COUNT is available.
enum MemoryPressureLevelUMA {
  UMA_MEMORY_PRESSURE_LEVEL_NONE = 0,
  UMA_MEMORY_PRESSURE_LEVEL_MODERATE = 1,
  UMA_MEMORY_PRESSURE_LEVEL_CRITICAL = 2,
  // This must be the last value in the enum.
  UMA_MEMORY_PRESSURE_LEVEL_COUNT,
};

// Enumeration of UMA pressure level changes. This needs to be kept in sync
// with histograms.xml and the memory pressure levels defined in
// MemoryPressureListener. Exposed for unittesting.
enum MemoryPressureLevelChangeUMA {
  UMA_MEMORY_PRESSURE_LEVEL_CHANGE_NONE_TO_MODERATE = 0,
  UMA_MEMORY_PRESSURE_LEVEL_CHANGE_NONE_TO_CRITICAL = 1,
  UMA_MEMORY_PRESSURE_LEVEL_CHANGE_MODERATE_TO_CRITICAL = 2,
  UMA_MEMORY_PRESSURE_LEVEL_CHANGE_CRITICAL_TO_MODERATE = 3,
  UMA_MEMORY_PRESSURE_LEVEL_CHANGE_CRITICAL_TO_NONE = 4,
  UMA_MEMORY_PRESSURE_LEVEL_CHANGE_MODERATE_TO_NONE = 5,
  // This must be the last value in the enum.
  UMA_MEMORY_PRESSURE_LEVEL_CHANGE_COUNT
};

// Class that is responsible for collecting and eventually reporting memory
// pressure statistics. Contributes to the "Memory.PressureLevel" and
// "Memory.PressureLevelChanges" histograms.
//
// On platforms with a polling memory pressure implementation the
// UpdateStatistics function will be invoked every time the pressure is polled.
// On non-polling platforms (Mac, Android) it will be invoked on a periodic
// timer, and at the moment of pressure level changes.
class MemoryPressureStatsCollector {
 public:
  using MemoryPressureLevel = MemoryPressureListener::MemoryPressureLevel;

  // The provided |tick_clock| must outlive this class.
  MemoryPressureStatsCollector(base::TickClock* tick_clock);

  // This is to be called periodically to ensure that up to date statistics
  // have been reported.
  void UpdateStatistics(MemoryPressureLevel current_pressure_level);

 private:
  friend class TestMemoryPressureStatsCollector;

  // Helper functions for delivering collected statistics.
  static void ReportCumulativeTime(MemoryPressureLevel pressure_level,
                                   int seconds);
  static void ReportLevelChange(MemoryPressureLevel old_pressure_level,
                                MemoryPressureLevel new_pressure_level);

  // The tick clock in use. This class is intended to be owned by a class with
  // a tick clock, and ownership remains there. Also intended as a test seam.
  base::TickClock* tick_clock_;

  // Buckets of time that have been spent in different pressure levels, but
  // not yet reported. At every call to UpdateStatistics these buckets will be
  // drained of as many 'full' seconds of time as possible and reported via
  // UMA. The remaining time (< 1000 ms) will be left in the bucket to be rolled
  // into a later UMA report.
  base::TimeDelta unreported_cumulative_time_[UMA_MEMORY_PRESSURE_LEVEL_COUNT];

  // The last observed pressure level and the time at which it was observed, and
  // the time when this pressure level started.
  MemoryPressureLevel last_pressure_level_;
  base::TimeTicks last_update_time_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureStatsCollector);
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_STATS_COLLECTOR_H_
