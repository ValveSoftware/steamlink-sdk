// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PROFILER_CONTENT_CONTENT_TRACKING_SYNCHRONIZER_DELEGATE_H_
#define COMPONENTS_METRICS_PROFILER_CONTENT_CONTENT_TRACKING_SYNCHRONIZER_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "components/metrics/profiler/tracking_synchronizer_delegate.h"
#include "content/public/browser/profiler_subscriber.h"

namespace metrics {

// Provides an implementation of TrackingSynchronizerDelegate for use on
// //content-based platforms. Specifically, drives the tracking of profiler data
// for child processes by interacting with content::ProfilerController.
class ContentTrackingSynchronizerDelegate : public TrackingSynchronizerDelegate,
                                            public content::ProfilerSubscriber {
 public:
  ~ContentTrackingSynchronizerDelegate() override;

  // Creates a ContentTrackingSynchronizerDelegate that is associated with
  // |synchronizer_|.
  static std::unique_ptr<TrackingSynchronizerDelegate> Create(
      TrackingSynchronizer* synchronizer);

 private:
  ContentTrackingSynchronizerDelegate(TrackingSynchronizer* synchronizer);

  // TrackingSynchronizerDelegate:
  void GetProfilerDataForChildProcesses(int sequence_number,
                                        int current_profiling_phase) override;
  void OnProfilingPhaseCompleted(int profiling_phase) override;

  // content::ProfilerSubscriber:
  void OnPendingProcesses(int sequence_number,
                          int pending_processes,
                          bool end) override;
  void OnProfilerDataCollected(
      int sequence_number,
      const tracked_objects::ProcessDataSnapshot& profiler_data,
      content::ProcessType process_type) override;

  TrackingSynchronizer* const synchronizer_;

  DISALLOW_COPY_AND_ASSIGN(ContentTrackingSynchronizerDelegate);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PROFILER_CONTENT_CONTENT_TRACKING_SYNCHRONIZER_DELEGATE_H_
