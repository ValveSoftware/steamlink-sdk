// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_DELEGATE_H_
#define COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_DELEGATE_H_


namespace metrics {

class TrackingSynchronizer;

// An abstraction of TrackingSynchronizer-related operations that depend on the
// platform.
class TrackingSynchronizerDelegate {
 public:
  virtual ~TrackingSynchronizerDelegate() {}

  // Should perform the platform-specific action that is needed to start
  // gathering profiler data for all relevant child processes.
  virtual void GetProfilerDataForChildProcesses(
      int sequence_number,
      int current_profiling_phase) = 0;

  // Called when |profiling_phase| has completed.
  virtual void OnProfilingPhaseCompleted(int profiling_phase) = 0;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_DELEGATE_H_
