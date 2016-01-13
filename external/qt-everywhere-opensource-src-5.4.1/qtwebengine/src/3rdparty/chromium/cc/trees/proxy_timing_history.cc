// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy_timing_history.h"

const size_t kDurationHistorySize = 60;
const double kCommitAndActivationDurationEstimationPercentile = 50.0;
const double kDrawDurationEstimationPercentile = 100.0;
const int kDrawDurationEstimatePaddingInMicroseconds = 0;

namespace cc {

ProxyTimingHistory::ProxyTimingHistory()
    : draw_duration_history_(kDurationHistorySize),
      begin_main_frame_to_commit_duration_history_(kDurationHistorySize),
      commit_to_activate_duration_history_(kDurationHistorySize) {}

ProxyTimingHistory::~ProxyTimingHistory() {}

base::TimeDelta ProxyTimingHistory::DrawDurationEstimate() const {
  base::TimeDelta historical_estimate =
      draw_duration_history_.Percentile(kDrawDurationEstimationPercentile);
  base::TimeDelta padding = base::TimeDelta::FromMicroseconds(
      kDrawDurationEstimatePaddingInMicroseconds);
  return historical_estimate + padding;
}

base::TimeDelta ProxyTimingHistory::BeginMainFrameToCommitDurationEstimate()
    const {
  return begin_main_frame_to_commit_duration_history_.Percentile(
      kCommitAndActivationDurationEstimationPercentile);
}

base::TimeDelta ProxyTimingHistory::CommitToActivateDurationEstimate() const {
  return commit_to_activate_duration_history_.Percentile(
      kCommitAndActivationDurationEstimationPercentile);
}

void ProxyTimingHistory::DidBeginMainFrame() {
  begin_main_frame_sent_time_ = base::TimeTicks::HighResNow();
}

void ProxyTimingHistory::DidCommit() {
  commit_complete_time_ = base::TimeTicks::HighResNow();
  begin_main_frame_to_commit_duration_history_.InsertSample(
      commit_complete_time_ - begin_main_frame_sent_time_);
}

void ProxyTimingHistory::DidActivatePendingTree() {
  commit_to_activate_duration_history_.InsertSample(
      base::TimeTicks::HighResNow() - commit_complete_time_);
}

void ProxyTimingHistory::DidStartDrawing() {
  start_draw_time_ = base::TimeTicks::HighResNow();
}

base::TimeDelta ProxyTimingHistory::DidFinishDrawing() {
  base::TimeDelta draw_duration =
      base::TimeTicks::HighResNow() - start_draw_time_;
  draw_duration_history_.InsertSample(draw_duration);
  return draw_duration;
}

}  // namespace cc
