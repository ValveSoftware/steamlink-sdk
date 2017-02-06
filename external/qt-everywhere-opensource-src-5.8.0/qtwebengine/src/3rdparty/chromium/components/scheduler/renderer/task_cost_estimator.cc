// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/task_cost_estimator.h"

#include "base/time/default_tick_clock.h"

namespace scheduler {

TaskCostEstimator::TaskCostEstimator(base::TickClock* time_source,
                                     int sample_count,
                                     double estimation_percentile)
    : rolling_time_delta_history_(sample_count),
      time_source_(time_source),
      outstanding_task_count_(0),
      estimation_percentile_(estimation_percentile) {}

TaskCostEstimator::~TaskCostEstimator() {}

void TaskCostEstimator::WillProcessTask(const base::PendingTask& pending_task) {
  // Avoid measuring the duration in nested run loops.
  if (++outstanding_task_count_ == 1)
    task_start_time_ = time_source_->NowTicks();
}

void TaskCostEstimator::DidProcessTask(const base::PendingTask& pending_task) {
  if (--outstanding_task_count_ == 0) {
    base::TimeDelta duration = time_source_->NowTicks() - task_start_time_;
    rolling_time_delta_history_.InsertSample(duration);
  }
}

base::TimeDelta TaskCostEstimator::expected_task_duration() const {
  return rolling_time_delta_history_.Percentile(estimation_percentile_);
}

void TaskCostEstimator::Clear() {
  rolling_time_delta_history_.Clear();
  expected_task_duration_ = base::TimeDelta();
}

}  // namespace scheduler
