// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_LOAD_TRACKER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_LOAD_TRACKER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "public/platform/WebCommon.h"

namespace blink {
namespace scheduler {

// This class tracks thread load level, i.e. percentage of wall time spent
// running tasks.
// In order to avoid bias it reports load level at regular intervals.
// Every |reporting_interval_| time units, it reports the average thread load
// level computed using a sliding window of width |reporting_interval_|.
class BLINK_PLATFORM_EXPORT ThreadLoadTracker {
 public:
  // Callback is called with (current_time, load_level) parameters.
  using Callback = base::Callback<void(base::TimeTicks, double)>;

  ThreadLoadTracker(base::TimeTicks now,
                    const Callback& callback,
                    base::TimeDelta reporting_interval,
                    base::TimeDelta waiting_period);
  ~ThreadLoadTracker();

  void Pause(base::TimeTicks now);
  void Resume(base::TimeTicks now);

  void RecordTaskTime(base::TimeTicks start_time, base::TimeTicks end_time);

  void RecordIdle(base::TimeTicks now);

  // TODO(altimin): Count wakeups.

 private:
  enum class ThreadState { ACTIVE, PAUSED };

  enum class TaskState { TASK_RUNNING, IDLE };

  // This function advances |time_| to |now|, calling |callback_|
  // in the process (multiple times if needed).
  void Advance(base::TimeTicks now, TaskState task_state);

  double Load();

  // |time_| is the last timestamp LoadTracker knows about.
  base::TimeTicks time_;
  base::TimeTicks next_reporting_time_;
  // Total period for which this LoadTracker was active. Needed to discard
  // first |waiting_period_| time.
  base::TimeDelta total_active_time_;

  ThreadState thread_state_;
  base::TimeTicks last_state_change_time_;

  // Start reporting values after |waiting_period_|.
  base::TimeDelta waiting_period_;
  base::TimeDelta reporting_interval_;

  // Recorded run time in window
  // [next_reporting_time - reporting_interval, next_reporting_time].
  base::TimeDelta run_time_inside_window_;

  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(ThreadLoadTracker);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_LOAD_TRACKER_H_
