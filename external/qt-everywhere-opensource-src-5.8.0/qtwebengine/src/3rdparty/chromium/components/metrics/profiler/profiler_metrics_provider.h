// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PROFILER_PROFILER_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_PROFILER_PROFILER_METRICS_PROVIDER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/profiler/tracking_synchronizer_observer.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"

namespace tracked_objects {
struct ProcessDataPhaseSnapshot;
}

namespace metrics {

// ProfilerMetricsProvider is responsible for filling out the |profiler_event|
// section of the UMA proto.
class ProfilerMetricsProvider : public MetricsProvider {
 public:
  explicit ProfilerMetricsProvider(
      const base::Callback<bool(void)>& cellular_callback);
  // Creates profiler metrics provider with a null callback.
  ProfilerMetricsProvider();
  ~ProfilerMetricsProvider() override;

  // MetricsDataProvider:
  void ProvideGeneralMetrics(ChromeUserMetricsExtension* uma_proto) override;

  // Records the passed profiled data, which should be a snapshot of the
  // browser's profiled performance during startup for a single process.
  void RecordProfilerData(
      const tracked_objects::ProcessDataPhaseSnapshot& process_data,
      base::ProcessId process_id,
      ProfilerEventProto::TrackedObject::ProcessType process_type,
      int profiling_phase,
      base::TimeDelta phase_start,
      base::TimeDelta phase_finish,
      const ProfilerEvents& past_events);

 private:
  // Returns true if current connection type is cellular and user is assigned to
  // experimental group for enabled cellular uploads according to
  // |cellular_callback_|.
  bool IsCellularLogicEnabled();

  // Saved cache of generated Profiler event protos, to be copied into the UMA
  // proto when ProvideGeneralMetrics() is called. The map is from a profiling
  // phase id to the profiler event proto that represents profiler data for the
  // profiling phase.
  std::map<int, ProfilerEventProto> profiler_events_cache_;

  // Returns true if current connection type is cellular and user is assigned to
  // experimental group for enabled cellular uploads.
  base::Callback<bool(void)> cellular_callback_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PROFILER_PROFILER_METRICS_PROVIDER_H_
