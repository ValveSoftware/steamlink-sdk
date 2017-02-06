// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_helper.h"

#include "base/macros.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

class StabilityMetricsHelperTest : public testing::Test {
 protected:
  StabilityMetricsHelperTest() : prefs_(new TestingPrefServiceSimple) {
    StabilityMetricsHelper::RegisterPrefs(prefs()->registry());
  }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;

  DISALLOW_COPY_AND_ASSIGN(StabilityMetricsHelperTest);
};

}  // namespace

TEST_F(StabilityMetricsHelperTest, BrowserChildProcessCrashed) {
  StabilityMetricsHelper helper(prefs());

  helper.BrowserChildProcessCrashed();
  helper.BrowserChildProcessCrashed();

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  metrics::SystemProfileProto system_profile;

  helper.ProvideStabilityMetrics(&system_profile);

  // Check current number of instances created.
  const metrics::SystemProfileProto_Stability& stability =
      system_profile.stability();

  EXPECT_EQ(2, stability.child_process_crash_count());
}

TEST_F(StabilityMetricsHelperTest, LogRendererCrash) {
  StabilityMetricsHelper helper(prefs());

  // Crash and abnormal termination should increment renderer crash count.
  helper.LogRendererCrash(false, base::TERMINATION_STATUS_PROCESS_CRASHED, 1);

  helper.LogRendererCrash(false, base::TERMINATION_STATUS_ABNORMAL_TERMINATION,
                          1);

  // Kill does not increment renderer crash count.
  helper.LogRendererCrash(false, base::TERMINATION_STATUS_PROCESS_WAS_KILLED,
                          1);

  // Failed launch increments failed launch count.
  helper.LogRendererCrash(false, base::TERMINATION_STATUS_LAUNCH_FAILED, 1);

  metrics::SystemProfileProto system_profile;

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  helper.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(2, system_profile.stability().renderer_crash_count());
  EXPECT_EQ(1, system_profile.stability().renderer_failed_launch_count());
  EXPECT_EQ(0, system_profile.stability().extension_renderer_crash_count());

  helper.ClearSavedStabilityMetrics();

  // Crash and abnormal termination should increment extension crash count.
  helper.LogRendererCrash(true, base::TERMINATION_STATUS_PROCESS_CRASHED, 1);

  // Failed launch increments extension failed launch count.
  helper.LogRendererCrash(true, base::TERMINATION_STATUS_LAUNCH_FAILED, 1);

  system_profile.Clear();
  helper.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(0, system_profile.stability().renderer_crash_count());
  EXPECT_EQ(1, system_profile.stability().extension_renderer_crash_count());
  EXPECT_EQ(
      1, system_profile.stability().extension_renderer_failed_launch_count());
}

}  // namespace metrics
