// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_provider.h"

namespace metrics {

MetricsProvider::MetricsProvider() {
}

MetricsProvider::~MetricsProvider() {
}

void MetricsProvider::Init() {
}

void MetricsProvider::OnDidCreateMetricsLog() {
}

void MetricsProvider::OnRecordingEnabled() {
}

void MetricsProvider::OnRecordingDisabled() {
}

void MetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile_proto) {
}

bool MetricsProvider::HasInitialStabilityMetrics() {
  return false;
}

void MetricsProvider::ProvideInitialStabilityMetrics(
    SystemProfileProto* system_profile_proto) {
}

void MetricsProvider::ProvideStabilityMetrics(
    SystemProfileProto* system_profile_proto) {
}

void MetricsProvider::ClearSavedStabilityMetrics() {
}

void MetricsProvider::ProvideGeneralMetrics(
    ChromeUserMetricsExtension* uma_proto) {
}

void MetricsProvider::MergeHistogramDeltas() {
}

void MetricsProvider::RecordHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
}

void MetricsProvider::RecordInitialHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
}

}  // namespace metrics
