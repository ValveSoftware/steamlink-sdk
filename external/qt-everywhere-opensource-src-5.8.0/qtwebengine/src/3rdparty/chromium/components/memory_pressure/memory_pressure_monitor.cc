// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/memory_pressure_monitor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "base/time/tick_clock.h"
#include "components/memory_pressure/memory_pressure_calculator.h"
#include "components/memory_pressure/memory_pressure_stats_collector.h"

namespace memory_pressure {

namespace {

using MemoryPressureLevel = MemoryPressureMonitor::MemoryPressureLevel;
const MemoryPressureLevel MEMORY_PRESSURE_LEVEL_NONE =
    MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
const MemoryPressureLevel MEMORY_PRESSURE_LEVEL_MODERATE =
    MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
const MemoryPressureLevel MEMORY_PRESSURE_LEVEL_CRITICAL =
    MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;

// Returns the polling/notification interval for the given pressure level.
int GetPollingIntervalMs(MemoryPressureLevel level) {
  switch (level) {
    case MEMORY_PRESSURE_LEVEL_NONE:
      return MemoryPressureMonitor::kDefaultPollingIntervalMs;

    case MEMORY_PRESSURE_LEVEL_MODERATE:
      return MemoryPressureMonitor::kNotificationIntervalPressureModerateMs;

    case MEMORY_PRESSURE_LEVEL_CRITICAL:
      return MemoryPressureMonitor::kNotificationIntervalPressureCriticalMs;
  }

  NOTREACHED();
  return 0;
}

base::TimeDelta GetPollingInterval(MemoryPressureLevel level) {
  return base::TimeDelta::FromMilliseconds(GetPollingIntervalMs(level));
}

// Serial number reserved for unscheduled checks as a result of external calls
// to the monitor.
const int kUnscheduledCheckSerial = 0;

}  // namespace

#if !defined(MEMORY_PRESSURE_IS_POLLING)
// Default definition of this class.
// TODO(chrisha): Implement useful versions of this for affected platforms.
class MemoryPressureMonitorImpl {};
#endif

#if defined(MEMORY_PRESSURE_IS_POLLING)
MemoryPressureMonitor::MemoryPressureMonitor(
    const scoped_refptr<base::TaskRunner>& task_runner,
    base::TickClock* tick_clock,
    MemoryPressureStatsCollector* stats_collector,
    MemoryPressureCalculator* calculator,
    const DispatchCallback& dispatch_callback)
    : task_runner_(task_runner),
      tick_clock_(tick_clock),
      stats_collector_(stats_collector),
      calculator_(calculator),
      dispatch_callback_(dispatch_callback),
      current_memory_pressure_level_(MEMORY_PRESSURE_LEVEL_NONE),
      serial_number_(kUnscheduledCheckSerial),
      weak_ptr_factory_(this) {
  DCHECK(task_runner_.get());
  DCHECK(tick_clock_);
  DCHECK(stats_collector_);
  DCHECK(calculator_);
  DCHECK(!dispatch_callback_.is_null());

  Start();
}
#else  // MEMORY_PRESSURE_IS_POLLING
MemoryPressureMonitor::MemoryPressureMonitor(
    const scoped_refptr<base::TaskRunner>& task_runner,
    base::TickClock* tick_clock,
    MemoryPressureStatsCollector* stats_collector,
    const DispatchCallback& dispatch_callback,
    MemoryPressureLevel initial_pressure_level)
    : task_runner_(task_runner),
      tick_clock_(tick_clock),
      stats_collector_(stats_collector),
      dispatch_callback_(dispatch_callback),
      current_memory_pressure_level_(initial_pressure_level),
      serial_number_(kUnscheduledCheckSerial),
      weak_ptr_factory_(this) {
  DCHECK(task_runner_.get());
  DCHECK(tick_clock_);
  DCHECK(stats_collector_);
  DCHECK(!dispatch_callback_.is_null());

  Start();
}
#endif  // !MEMORY_PRESSURE_IS_POLLING

MemoryPressureMonitor::~MemoryPressureMonitor() {}

MemoryPressureLevel MemoryPressureMonitor::GetCurrentPressureLevel() {
  base::AutoLock lock(lock_);

#if defined(MEMORY_PRESSURE_IS_POLLING)
  // Force an immediate pressure check on polling platforms. On non-polling
  // platforms the current memory pressure is always valid.
  CheckPressureAndUpdateStatsLocked(kUnscheduledCheckSerial);
#endif

  return current_memory_pressure_level_;
}

void MemoryPressureMonitor::CheckMemoryPressureSoon() {
  // This function is a nop on non-polling platforms.
#if defined(MEMORY_PRESSURE_IS_POLLING)
  // Schedule a check to run as soon as possible.
  base::AutoLock lock(lock_);
  base::TimeTicks now = tick_clock_->NowTicks();
  ScheduleTaskLocked(now);
#endif
}

#if !defined(MEMORY_PRESSURE_IS_POLLING)
// This is the entry point for OS notifications of pressure level changes.
void MemoryPressureMonitor::OnMemoryPressureChanged(
    MemoryPressureLevel level) {
  base::AutoLock lock(lock_);

  // Do nothing if the level hasn't changed.
  if (level == current_memory_pressure_level_)
    return;

  // Update the level and the stats.
  current_memory_pressure_level_ = level;
  stats_collector_->UpdateStatistics(current_memory_pressure_level_);

  // Only dispatch notifications if there is memory pressure.
  if (current_memory_pressure_level_ > MEMORY_PRESSURE_LEVEL_NONE) {
    last_notification_ = tick_clock_->NowTicks();
    dispatch_callback_.Run(current_memory_pressure_level_);
  }

  ScheduleTaskIfNeededLocked(kUnscheduledCheckSerial);
}
#endif

void MemoryPressureMonitor::Start() {
  base::AutoLock lock(lock_);

  base::TimeTicks now = tick_clock_->NowTicks();

  // Get the statistics collector warmed up by measuring the pressure level.
  // Don't immediately fire a signal if already under memory pressure as the
  // system is quite busy with startup right now. Wait until the first
  // renotification is required, allowing the browser time to get started.
  // Non-polling implementations set the initial memory pressure level in the
  // constructor.
#if defined(MEMORY_PRESSURE_IS_POLLING)
  current_memory_pressure_level_ = calculator_->CalculateCurrentPressureLevel();
  last_check_ = now;
#endif

  last_notification_ = now;
  stats_collector_->UpdateStatistics(current_memory_pressure_level_);
  ScheduleTaskIfNeededLocked(kUnscheduledCheckSerial);
}

void MemoryPressureMonitor::CheckPressureAndUpdateStats(int serial) {
  // This should only ever be used by scheduled checks.
  DCHECK_NE(kUnscheduledCheckSerial, serial);
  base::AutoLock lock(lock_);
  CheckPressureAndUpdateStatsLocked(serial);
}

void MemoryPressureMonitor::CheckPressureAndUpdateStatsLocked(int serial) {
  lock_.AssertAcquired();

  base::TimeTicks now = tick_clock_->NowTicks();
  MemoryPressureLevel old_level = current_memory_pressure_level_;

#if defined(MEMORY_PRESSURE_IS_POLLING)
  // Don't check again if pressure was calculated too recently.
  DCHECK(!last_check_.is_null());
  if ((now - last_check_) >=
      base::TimeDelta::FromMilliseconds(kMinimumTimeBetweenSamplesMs)) {
    // Calculate the current pressure level and update statistics.
    last_check_ = now;
    current_memory_pressure_level_ =
        calculator_->CalculateCurrentPressureLevel();
    stats_collector_->UpdateStatistics(current_memory_pressure_level_);
  }
#else  // MEMORY_PRESSURE_IS_POLLING
  // On non-polling platforms this function can only be invoked by scheduled
  // callbacks for updating statistics and sending renotifications. Simply
  // update the statistics and move on.
  DCHECK_NE(kUnscheduledCheckSerial, serial);
  stats_collector_->UpdateStatistics(current_memory_pressure_level_);
#endif  // !MEMORY_PRESSURE_IS_POLLING

  // Check if the level has changed or a renotification is required.
  DCHECK(!last_notification_.is_null());
  if (current_memory_pressure_level_ != old_level ||
      now - last_notification_ >=
          GetPollingInterval(current_memory_pressure_level_)) {
    last_notification_ = now;

    // Only dispatch notifications if there is memory pressure.
    if (current_memory_pressure_level_ > MEMORY_PRESSURE_LEVEL_NONE)
      dispatch_callback_.Run(current_memory_pressure_level_);
  }

  ScheduleTaskIfNeededLocked(serial);
}

void MemoryPressureMonitor::ScheduleTaskIfNeededLocked(int serial) {
  lock_.AssertAcquired();

  // Remove this check from the set of scheduled checks.
  if (serial != kUnscheduledCheckSerial) {
    size_t erased = scheduled_checks_.erase(serial);
    DCHECK_EQ(1u, erased);
  }

  // Get the time of the soonest scheduled check. This linear scan is quick
  // because the map will contain at most 1 entry per pressure level, and most
  // commonly only 1 entry.
  base::TimeTicks next_check;
  for (const auto& check : scheduled_checks_) {
    if (next_check.is_null() || check.second < next_check)
      next_check = check.second;
  }

  // Get the time of the required next check.
  base::TimeTicks required_check =
      last_notification_ + GetPollingInterval(current_memory_pressure_level_);

  // If there's already a check scheduled in time then don't schedule
  // another. This lets the number of scheduled checks shrink back down to 1
  // as pressure changes occur.
  if (!next_check.is_null() && next_check <= required_check)
    return;

  ScheduleTaskLocked(required_check);
}

void MemoryPressureMonitor::ScheduleTaskLocked(base::TimeTicks when) {
  lock_.AssertAcquired();
  int serial = ++serial_number_;

  // Handle overflow. For simplicity keep serial numbers positive. 2^31 leaves
  // room for 20 years of uptime in the worst case scenario (poll every second,
  // constantly swinging through all the pressure levels). But you know...
  // better safe the sorry!
  if (serial < 0) {
    serial_number_ = 1;
    serial = 1;
  }

  // It's entirely possible for |when| to be in the past relative to |now|, so
  // bound |delay| from below.
  base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeDelta delay;  // Initializes to zero.
  if (when > now)
    delay = when - now;

  // Schedule another check.
  if (task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&MemoryPressureMonitor::CheckPressureAndUpdateStats,
                     weak_ptr_factory_.GetWeakPtr(), serial),
          delay)) {
    // If the task will run then add it to the map of scheduled checks.
    scheduled_checks_[serial] = when;
  }
}

}  // namespace memory_pressure
