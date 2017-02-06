// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_METRICS_PROVIDER_H_

#include "base/macros.h"

namespace base {
class HistogramSnapshotManager;
}  // namespace base

namespace metrics {

class ChromeUserMetricsExtension;
class SystemProfileProto;
class SystemProfileProto_Stability;

// MetricsProvider is an interface allowing different parts of the UMA protos to
// be filled out by different classes.
class MetricsProvider {
 public:
  MetricsProvider();
  virtual ~MetricsProvider();

  // Called after initialiazation of MetricsService and field trials.
  virtual void Init();

  // Called when a new MetricsLog is created.
  virtual void OnDidCreateMetricsLog();

  // Called when metrics recording has been enabled.
  virtual void OnRecordingEnabled();

  // Called when metrics recording has been disabled.
  virtual void OnRecordingDisabled();

  // Provides additional metrics into the system profile.
  virtual void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto);

  // Called once at startup to see whether this provider has critical stability
  // events to share in an initial stability log.
  // Returning true can trigger ProvideInitialStabilityMetrics and
  // ProvideStabilityMetrics on all other registered metrics providers.
  // Default implementation always returns false.
  virtual bool HasInitialStabilityMetrics();

  // Called at most once at startup when an initial stability log is created.
  // It provides critical statiblity metrics that need to be reported in an
  // initial stability log.
  // Default implementation is a no-op.
  virtual void ProvideInitialStabilityMetrics(
      SystemProfileProto* system_profile_proto);

  // Provides additional stability metrics. Stability metrics can be provided
  // directly into |stability_proto| fields or by logging stability histograms
  // via the UMA_STABILITY_HISTOGRAM_ENUMERATION() macro.
  virtual void ProvideStabilityMetrics(
      SystemProfileProto* system_profile_proto);

  // Called to indicate that saved stability prefs should be cleared, e.g.
  // because they are from an old version and should not be kept.
  virtual void ClearSavedStabilityMetrics();

  // Provides general metrics that are neither system profile nor stability
  // metrics. May also be used to add histograms when final metrics are
  // collected right before upload.
  virtual void ProvideGeneralMetrics(
      ChromeUserMetricsExtension* uma_proto);

  // Called during regular collection to explicitly merge histogram deltas
  // to the global StatisticsRecorder.
  virtual void MergeHistogramDeltas();

  // Called during regular collection to explicitly load histogram snapshots
  // using a snapshot manager. PrepareDeltas() will have already been called
  // and FinishDeltas() will be called later; calls to only PrepareDelta(),
  // not PrepareDeltas (plural), should be made.
  virtual void RecordHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager);

  // Called during collection of initial metrics to explicitly load histogram
  // snapshots using a snapshot manager. PrepareDeltas() will have already
  // been called and FinishDeltas() will be called later; calls to only
  // PrepareDelta(), not PrepareDeltas (plural), should be made.
  virtual void RecordInitialHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager);

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_PROVIDER_H_
