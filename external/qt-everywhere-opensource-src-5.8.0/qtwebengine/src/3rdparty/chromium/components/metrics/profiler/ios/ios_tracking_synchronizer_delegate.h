// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PROFILER_IOS_IOS_TRACKING_SYNCHRONIZER_DELEGATE_H_
#define COMPONENTS_METRICS_PROFILER_IOS_IOS_TRACKING_SYNCHRONIZER_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "components/metrics/profiler/tracking_synchronizer_delegate.h"

namespace metrics {

// Provides an implementation of TrackingSynchronizerDelegate for usage on iOS.
// This implementation is minimal, as on iOS there are no child processes.
class IOSTrackingSynchronizerDelegate : public TrackingSynchronizerDelegate {
 public:
  ~IOSTrackingSynchronizerDelegate() override;

  // Creates an IOSTrackingSynchronizerDelegate that is associated with
  // |synchronizer_|.
  static std::unique_ptr<TrackingSynchronizerDelegate> Create(
      TrackingSynchronizer* synchronizer);

 private:
  IOSTrackingSynchronizerDelegate(TrackingSynchronizer* synchronizer);

  // TrackingSynchronizerDelegate:
  void GetProfilerDataForChildProcesses(int sequence_number,
                                        int current_profiling_phase) override;
  void OnProfilingPhaseCompleted(int profiling_phase) override;

  TrackingSynchronizer* const synchronizer_;

  DISALLOW_COPY_AND_ASSIGN(IOSTrackingSynchronizerDelegate);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PROFILER_IOS_IOS_TRACKING_SYNCHRONIZER_DELEGATE_H_
