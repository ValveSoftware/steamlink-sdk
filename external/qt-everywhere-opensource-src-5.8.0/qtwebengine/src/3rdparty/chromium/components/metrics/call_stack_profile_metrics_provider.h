// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {
class ChromeUserMetricsExtension;

// Performs metrics logging for the stack sampling profiler.
class CallStackProfileMetricsProvider : public MetricsProvider {
 public:
  // The event that triggered the profile collection.
  // This enum should be kept in sync with content/common/profiled_stack_state.h
  enum Trigger {
    UNKNOWN,
    PROCESS_STARTUP,
    JANKY_TASK,
    THREAD_HUNG,
    TRIGGER_LAST = THREAD_HUNG
  };

  // Parameters to pass back to the metrics provider.
  struct Params {
    explicit Params(Trigger trigger);
    Params(Trigger trigger, bool preserve_sample_ordering);

    // The triggering event.
    Trigger trigger;

    // True if sample ordering is important and should be preserved when the
    // associated profiles are compressed. This should only be set to true if
    // the intended use of the requires that the sequence of call stacks within
    // a particular profile be preserved. The default value of false provides
    // better compression of the encoded profile and is sufficient for the
    // typical use case of recording profiles for stack frequency analysis in
    // aggregate.
    bool preserve_sample_ordering;
  };

  CallStackProfileMetricsProvider();
  ~CallStackProfileMetricsProvider() override;

  // Get a callback for use with StackSamplingProfiler that provides completed
  // profiles to this object. The callback should be immediately passed to the
  // StackSamplingProfiler, and should not be reused between
  // StackSamplingProfilers. This function may be called on any thread.
  static base::StackSamplingProfiler::CompletedCallback GetProfilerCallback(
      const Params& params);

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
