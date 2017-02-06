// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_OBSERVER_H_
#define COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_OBSERVER_H_

#include <vector>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"

namespace base {
class TimeDelta;
}

namespace tracked_objects {
struct ProcessDataPhaseSnapshot;
}

namespace metrics {

// Set of profiling events, in no guaranteed order. Implemented as a vector
// because we don't need to have an efficient .find() on it, so vector<> is more
// efficient.
typedef std::vector<ProfilerEventProto::ProfilerEvent> ProfilerEvents;

// Attributes of profiler data passed to
// TrackingSynchronizerObserver::ReceivedProfilerData.
struct ProfilerDataAttributes {
  ProfilerDataAttributes(
      int profiling_phase,
      base::ProcessId process_id,
      ProfilerEventProto::TrackedObject::ProcessType process_type,
      base::TimeTicks phase_start,
      base::TimeTicks phase_finish);

  // 0-indexed profiling phase number.
  const int profiling_phase;

  // ID of the process that reported the data.
  const base::ProcessId process_id;

  // Type of the process that reported the data.
  const ProfilerEventProto::TrackedObject::ProcessType process_type;

  // Time of the profiling phase start.
  const base::TimeTicks phase_start;

  // Time of the profiling phase finish.
  const base::TimeTicks phase_finish;
};

// Observer for notifications from the TrackingSynchronizer class.
class TrackingSynchronizerObserver {
 public:
  // Received |process_data_phase| for profiling phase and process defined by
  // |attributes|.
  // Each completed phase is associated with an event that triggered the
  // completion of the phase. |past_events| contains the set of events that
  // completed prior to the reported phase. This data structure is useful for
  // quickly computing the full set of profiled traces that occurred before or
  // after a given event.
  // The observer should assume there might be more data coming until
  // FinishedReceivingData() is called.
  virtual void ReceivedProfilerData(
      const ProfilerDataAttributes& attributes,
      const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
      const ProfilerEvents& past_events) = 0;

  // The observer should not expect any more calls to |ReceivedProfilerData()|
  // (without re-registering).  This is sent either when data from all processes
  // has been gathered, or when the request times out.
  virtual void FinishedReceivingProfilerData() {}

 protected:
  TrackingSynchronizerObserver() {}
  virtual ~TrackingSynchronizerObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TrackingSynchronizerObserver);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_OBSERVER_H_
