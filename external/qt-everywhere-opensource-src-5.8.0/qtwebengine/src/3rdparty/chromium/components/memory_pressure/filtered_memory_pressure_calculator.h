// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_FILTERED_MEMORY_PRESSURE_CALCULATOR_H_
#define COMPONENTS_MEMORY_PRESSURE_FILTERED_MEMORY_PRESSURE_CALCULATOR_H_

#include "components/memory_pressure/memory_pressure_calculator.h"

#include "base/macros.h"
#include "base/time/time.h"

namespace base {
class TickClock;
}  // namespace base

namespace memory_pressure {

#if defined(MEMORY_PRESSURE_IS_POLLING)

// A utility class that provides rate-limiting and hysteresis on raw memory
// pressure calculations. This is identical across all platforms, but only used
// on those that do not have native memory pressure signals.
class FilteredMemoryPressureCalculator : public MemoryPressureCalculator {
 public:
  // The cooldown period when transitioning from critical to moderate/no memory
  // pressure, or from moderate to none.
  //
  // These values were experimentally obtained during the initial ChromeOS only
  // implementation of this feature. By spending a significant cooldown period
  // at a higher pressure level more time is dedicated to freeing resources and
  // less churn occurs in the MemoryPressureListener event stream.
  enum : int { kCriticalPressureCooldownPeriodMs = 5000 };
  enum : int { kModeratePressureCooldownPeriodMs = 5000 };

  // The provided |pressure_calculator| and |tick_clock| must outlive this
  // object.
  FilteredMemoryPressureCalculator(
      MemoryPressureCalculator* pressure_calculator,
      base::TickClock* tick_clock);
  ~FilteredMemoryPressureCalculator() override;

  // Calculates the current pressure level.
  MemoryPressureLevel CalculateCurrentPressureLevel() override;

  // Accessors for unittesting.
  bool cooldown_in_progress() const { return cooldown_in_progress_; }
  base::TimeTicks cooldown_start_time() const { return cooldown_start_time_; }
  MemoryPressureLevel cooldown_high_tide() const { return cooldown_high_tide_; }

 private:
  friend class TestFilteredMemoryPressureCalculator;

  // The delegate pressure calculator. Provided by the constructor.
  MemoryPressureCalculator* pressure_calculator_;

  // The delegate tick clock. Provided by the constructor.
  base::TickClock* tick_clock_;

  // The memory pressure currently being reported.
  MemoryPressureLevel current_pressure_level_;

  // The last time a sample was taken.
  bool samples_taken_;
  base::TimeTicks last_sample_time_;

  // State of an ongoing cooldown period, if any. The high-tide line indicates
  // the highest memory pressure level (*below* the current one) that was
  // encountered during the cooldown period. This allows a cooldown to
  // transition directly from critical to none if *no* moderate pressure signals
  // were seen during the period, otherwise it forces it to pass through a
  // moderate cooldown as well.
  bool cooldown_in_progress_;
  base::TimeTicks cooldown_start_time_;
  MemoryPressureLevel cooldown_high_tide_;

  DISALLOW_COPY_AND_ASSIGN(FilteredMemoryPressureCalculator);
};

#endif  // defined(MEMORY_PRESSURE_IS_POLLING)

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_FILTERED_MEMORY_PRESSURE_CALCULATOR_H_
