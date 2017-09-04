// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Declares the MemoryPressureMonitor class. This is responsible for monitoring
// system-wide memory pressure and dispatching memory pressure signals to
// MemoryPressureListener. It is also responsible for rate limiting calls to the
// memory pressure subsytem and gathering statistics for UMA.
//
// The class has a few compile time differences depending on if
// MEMORY_PRESSURE_IS_POLLING is defined. For Windows, ChromeOS and Linux
// the implementation is polling so MEMORY_PRESSURE_IS_POLLING is defined. For
// Mac, iOS and Android it is not defined.
//
// The difference is that "polling" platforms have no native OS signals
// indicating memory pressure. These platforms implement
// DirectMemoryPressureCalculator, which is polled on a schedule to check for
// memory pressure changes. On non-polling platforms the OS provides a native
// signal. This signal is observed by the platform-specific implementation of
// MemoryPressureMonitorImpl.
//
// The memory pressure system periodically repeats memory pressure signals while
// under memory pressure (the interval varying depending on the pressure level).
// As such, even non-polling platforms require a scheduling mechanism for
// repeating notifications. Both implementations share this basic scheduling
// subsystem, and also leverage it to make timely UMA reports.

#ifndef COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_MONITOR_H_
#define COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_MONITOR_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "components/memory_pressure/memory_pressure_listener.h"

namespace base {
class TaskRunner;
class TickClock;
}  // namespace

namespace memory_pressure {

class MemoryPressureCalculator;
class MemoryPressureStatsCollector;

#if !defined(MEMORY_PRESSURE_IS_POLLING)
// For non-polling platform specific implementation details. An instance of
// this class will be encapsulated in the monitor. It will received an injected
// callback that routes messages to OnMemoryPressureChanged.
class MemoryPressureMonitorImpl;
#endif

// A thread-safe class for directly querying and automatically monitoring
// memory pressure. When system memory pressure levels change this class is
// responsible for notifying MemoryPressureListeners, and periodically
// renotifying them as conditions persist. This class will do its periodic work
// on the thread on which it was created. However, it is safe to call
// GetCurrentPressureLevel from any thread.
//
// This class doesn't make use of base::RepeatingTimer as it leaves an orphaned
// scheduled task every time it is canceled. This can occur every time a memory
// pressure level transition occurs, which has no strict upper bound. Instead
// a collection of at most "number of memory pressure level" scheduled tasks
// is used, with these tasks being reused as transition levels are crossed and
// polling requirements change. See |scheduled_checks_| and
// "ScheduleTaskIfNeededLocked" for details.
class MemoryPressureMonitor {
 public:
  using MemoryPressureLevel = MemoryPressureListener::MemoryPressureLevel;

  // A simple dispatch delegate as a testing seam. Makes unittests much simpler
  // as they don't need to setup a multithreaded environment.
  using DispatchCallback = base::Callback<void(MemoryPressureLevel)>;

#if defined(MEMORY_PRESSURE_IS_POLLING)
  // The minimum time that must pass between successive polls. This enforces an
  // upper bound on the rate of calls to the contained MemoryPressureCalculator.
  // 100ms (10Hz) allows a relatively fast respsonse time for rapidly increasing
  // memory usage, but limits the amount of work done in the calculator and
  // stats collection.
  enum : int { kMinimumTimeBetweenSamplesMs = 100 };
  // On polling platforms this is required to be somewhat short in order to
  // observe memory pressure changes as they occur.
  enum : int { kDefaultPollingIntervalMs = 5000 };
#else
  // On non-polling platforms this is only required for keeping statistics up to
  // date so can be quite a long period.
  enum : int { kDefaultPollingIntervalMs = 60000 };
#endif

  // Renotification intervals, per pressure level. These are the same on all
  // platforms. These act as an upper bound on the polling interval when under
  // the corresponding memory pressure.
  enum : int { kNotificationIntervalPressureModerateMs = 5000 };
  enum : int { kNotificationIntervalPressureCriticalMs = 1000 };

#if defined(MEMORY_PRESSURE_IS_POLLING)
  // Fully configurable constructor for polling platforms.
  MemoryPressureMonitor(const scoped_refptr<base::TaskRunner>& task_runner,
                        base::TickClock* tick_clock,
                        MemoryPressureStatsCollector* stats_collector,
                        MemoryPressureCalculator* calculator,
                        const DispatchCallback& dispatch_callback);
#else
  // Constructor for non-polling platforms.
  MemoryPressureMonitor(const scoped_refptr<base::TaskRunner>& task_runner,
                        base::TickClock* tick_clock,
                        MemoryPressureStatsCollector* stats_collector,
                        const DispatchCallback& dispatch_callback,
                        MemoryPressureLevel initial_pressure_level);
#endif

  ~MemoryPressureMonitor();

  // Returns the current memory pressure level. On polling platforms this may
  // result in a forced calculation of the current pressure level (a cheap
  // operation). Can be called from any thread.
  MemoryPressureLevel GetCurrentPressureLevel();

  // Schedules a memory pressure check to run soon. This can be called from any
  // thread.
  void CheckMemoryPressureSoon();

 private:
  // For unittesting.
  friend class TestMemoryPressureMonitor;

#if !defined(MEMORY_PRESSURE_IS_POLLING)
  // Notifications from the OS will be routed here by the contained
  // MemoryPressureMonitorImpl instance. For statistics and renotification to
  // work properly this must be notified of all pressure level changes, even
  // those indicating a return to a state of no pressure.
  void OnMemoryPressureChanged(MemoryPressureLevel level);
#endif

  // Starts the memory pressure monitor. To be called in the constructor.
  void Start();

  // Checks memory pressure and updates stats. This is the entry point for all
  // scheduled checks. Each scheduled check is assigned a |serial| id
  // (monotonically increasing) which is used to tie the task to the time at
  // which it was scheduled via the |scheduled_checks_| map.
  void CheckPressureAndUpdateStats(int serial);
  void CheckPressureAndUpdateStatsLocked(int serial);

  // Ensures that a task is scheduled for renotification/recalculation/stats
  // updating. Uses the |serial| id of the current task to determine the time at
  // which the next scheduled check should run. Assumes |lock_| is held.
  void ScheduleTaskIfNeededLocked(int serial);

  // Schedules a task.
  void ScheduleTaskLocked(base::TimeTicks when);

  // A lock for synchronization.
  base::Lock lock_;

  // Injected dependencies.
  // The task runner on which periodic pressure checks and statistics uploading
  // are run.
  scoped_refptr<base::TaskRunner> task_runner_;
  // The tick clock in use. Used under |lock_|.
  base::TickClock* tick_clock_;
  // The stats collector in use. Used under |lock_|.
  MemoryPressureStatsCollector* stats_collector_;
  // The memory pressure calculator in use. Used under |lock_|.
  MemoryPressureCalculator* calculator_;
  // The dispatch callback to use.
  DispatchCallback dispatch_callback_;

#if !defined(MEMORY_PRESSURE_IS_POLLING)
  // On non-polling platforms this object is responsible for routing OS
  // notifications to OnMemoryPressureChanged, and setting the initial pressure
  // value. The OS specific implementation is responsible for allocating this
  // object.
  std::unique_ptr<MemoryPressureMonitorImpl> monitor_impl_;
#endif

  // Object state.
  // The pressure level as of the most recent poll or notification. Under
  // |lock_|.
  MemoryPressureLevel current_memory_pressure_level_;
#if defined(MEMORY_PRESSURE_IS_POLLING)
  // Time of the last pressure check. Under |lock_|. Only needed for polling
  // implementations.
  base::TimeTicks last_check_;
#endif
  // Time of the last pressure notification. Under |lock_|.
  base::TimeTicks last_notification_;
  // A map of scheduled pressure checks/statistics updates and their serial
  // numbers. Under |lock_|.
  std::map<int, base::TimeTicks> scheduled_checks_;
  // The most recently assigned serial number for a pressure check. The number
  // 0 will never be assigned but will instead be reserved for unscheduled
  // checks initiated externally. Under |lock_|.
  int serial_number_;

  // Weak pointer factory to ourself used for posting delayed tasks to
  // task_runner_.
  base::WeakPtrFactory<MemoryPressureMonitor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureMonitor);
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_MONITOR_H_
