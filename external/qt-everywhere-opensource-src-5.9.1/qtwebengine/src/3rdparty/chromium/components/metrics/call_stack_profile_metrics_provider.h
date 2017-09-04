// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {
class ChromeUserMetricsExtension;

// Performs metrics logging for the stack sampling profiler.
class CallStackProfileMetricsProvider : public MetricsProvider {
 public:
  CallStackProfileMetricsProvider();
  ~CallStackProfileMetricsProvider() override;

  // Get a callback for use with StackSamplingProfiler that provides completed
  // profiles to this object. The callback should be immediately passed to the
  // StackSamplingProfiler, and should not be reused between
  // StackSamplingProfilers. This function may be called on any thread.
  static base::StackSamplingProfiler::CompletedCallback GetProfilerCallback(
      const CallStackProfileParams& params);

  // Provides completed stack profiles to the metrics provider. Intended for use
  // when receiving profiles over IPC. In-process StackSamplingProfiler users
  // should use GetProfilerCallback() instead. |profiles| is not const& because
  // it must be passed with std::move.
  static void ReceiveCompletedProfiles(
      const CallStackProfileParams& params,
      base::TimeTicks start_timestamp,
      base::StackSamplingProfiler::CallStackProfiles profiles);

  // MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideGeneralMetrics(ChromeUserMetricsExtension* uma_proto) override;

 protected:
  // Finch field trial and group for reporting profiles. Provided here for test
  // use.
  static const char kFieldTrialName[];
  static const char kReportProfilesGroupName[];

  // Reset the static state to the defaults after startup.
  static void ResetStaticStateForTesting();

 private:
  // Returns true if reporting of profiles is enabled according to the
  // controlling Finch field trial.
  static bool IsReportingEnabledByFieldTrial();

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
