// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/profiler/ios/ios_tracking_synchronizer_delegate.h"

#include "base/memory/ptr_util.h"
#include "components/metrics/profiler/tracking_synchronizer.h"

namespace metrics {

// static
std::unique_ptr<TrackingSynchronizerDelegate>
IOSTrackingSynchronizerDelegate::Create(TrackingSynchronizer* synchronizer) {
  return base::WrapUnique(new IOSTrackingSynchronizerDelegate(synchronizer));
}

IOSTrackingSynchronizerDelegate::IOSTrackingSynchronizerDelegate(
    TrackingSynchronizer* synchronizer)
    : synchronizer_(synchronizer) {
  DCHECK(synchronizer_);
}

IOSTrackingSynchronizerDelegate::~IOSTrackingSynchronizerDelegate() {}

void IOSTrackingSynchronizerDelegate::GetProfilerDataForChildProcesses(
    int sequence_number,
    int current_profiling_phase) {
  // Notify |synchronizer_| that there are no processes pending (on iOS, there
  // is only the browser process).
  synchronizer_->OnPendingProcesses(sequence_number, 0, true);
}

void IOSTrackingSynchronizerDelegate::OnProfilingPhaseCompleted(
    int profiling_phase) {}

}  // namespace metrics
