// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/queueing_time_estimator.h"

#include "base/time/default_tick_clock.h"

#include <algorithm>

namespace blink {
namespace scheduler {

namespace {

// This method computes the expected queueing time of a randomly distributed
// task R within a window containing a single task T. Let T' be the time range
// for which T overlaps the window. We first compute the probability that R will
// start within T'. We then compute the expected queueing duration if R does
// start within this range. Since the start time of R is uniformly distributed
// within the window, this is equal to the average of the queueing times if R
// started at the beginning or end of T'. The expected queueing time of T is the
// probability that R will start within T', multiplied by the expected queueing
// duration if R does fall in this range.
base::TimeDelta ExpectedQueueingTimeFromTask(base::TimeTicks task_start,
                                             base::TimeTicks task_end,
                                             base::TimeTicks window_start,
                                             base::TimeTicks window_end) {
  DCHECK(task_start <= task_end);
  DCHECK(task_start <= window_end);
  DCHECK(window_start < window_end);
  DCHECK(task_end >= window_start);
  base::TimeTicks task_in_window_start_time =
      std::max(task_start, window_start);
  base::TimeTicks task_in_window_end_time = std::min(task_end, window_end);
  DCHECK(task_in_window_end_time <= task_in_window_end_time);

  double probability_of_this_task =
      static_cast<double>((task_in_window_end_time - task_in_window_start_time)
                              .InMicroseconds()) /
      (window_end - window_start).InMicroseconds();

  base::TimeDelta expected_queueing_duration_within_task =
      ((task_end - task_in_window_start_time) +
       (task_end - task_in_window_end_time)) /
      2;

  return base::TimeDelta::FromMillisecondsD(
      probability_of_this_task *
      expected_queueing_duration_within_task.InMillisecondsF());
}

}  // namespace

QueueingTimeEstimator::QueueingTimeEstimator(
    QueueingTimeEstimator::Client* client,
    base::TimeDelta window_duration)
    : client_(client),
      window_duration_(window_duration),
      window_start_time_() {}

void QueueingTimeEstimator::OnToplevelTaskCompleted(
    base::TimeTicks task_start_time,
    base::TimeTicks task_end_time) {
  if (window_start_time_.is_null())
    window_start_time_ = task_start_time;

  while (TimePastWindowEnd(task_end_time)) {
    if (!TimePastWindowEnd(task_start_time)) {
      // Include the current task in this window.
      current_expected_queueing_time_ += ExpectedQueueingTimeFromTask(
          task_start_time, task_end_time, window_start_time_,
          window_start_time_ + window_duration_);
    }
    client_->OnQueueingTimeForWindowEstimated(current_expected_queueing_time_);
    window_start_time_ += window_duration_;
    current_expected_queueing_time_ = base::TimeDelta();
  }

  current_expected_queueing_time_ += ExpectedQueueingTimeFromTask(
      task_start_time, task_end_time, window_start_time_,
      window_start_time_ + window_duration_);
}

bool QueueingTimeEstimator::TimePastWindowEnd(base::TimeTicks time) {
  return time > window_start_time_ + window_duration_;
}

}  // namespace scheduler
}  // namespace blink
