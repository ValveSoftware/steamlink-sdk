// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/watcher_metrics_provider_win.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdlib>

#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_simple_task_runner.h"
#include "base/win/registry.h"
#include "components/browser_watcher/exit_funnel_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

namespace {

const wchar_t kRegistryPath[] = L"Software\\WatcherMetricsProviderWinTest";

class WatcherMetricsProviderWinTest : public testing::Test {
 public:
  typedef testing::Test Super;

  void SetUp() override {
    Super::SetUp();

    override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
    test_task_runner_ = new base::TestSimpleTaskRunner();
  }

  void AddProcessExitCode(bool use_own_pid, int exit_code) {
    int pid = 0;
    if (use_own_pid) {
      pid = base::GetCurrentProcId();
    } else {
      // Make sure not to accidentally collide with own pid.
      do {
        pid = rand();
      } while (pid == static_cast<int>(base::GetCurrentProcId()));
    }

    base::win::RegKey key(HKEY_CURRENT_USER, kRegistryPath, KEY_WRITE);

    // Make up a unique key, starting with the given pid.
    base::string16 key_name(base::StringPrintf(L"%d-%d", pid, rand()));

    // Write the exit code to registry.
    LONG result = key.WriteValue(key_name.c_str(), exit_code);
    ASSERT_EQ(result, ERROR_SUCCESS);
  }

  size_t ExitCodeRegistryPathValueCount() {
    base::win::RegKey key(HKEY_CURRENT_USER, kRegistryPath, KEY_READ);
    return key.GetValueCount();
  }

  void AddExitFunnelEvent(int pid, const base::char16* name, int64_t value) {
    base::string16 key_name =
        base::StringPrintf(L"%ls\\%d-%d", kRegistryPath, pid, pid);

    base::win::RegKey key(HKEY_CURRENT_USER, key_name.c_str(), KEY_WRITE);
    ASSERT_EQ(key.WriteValue(name, &value, sizeof(value), REG_QWORD),
              ERROR_SUCCESS);
  }

 protected:
  registry_util::RegistryOverrideManager override_manager_;
  base::HistogramTester histogram_tester_;
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;
};

}  // namespace

TEST_F(WatcherMetricsProviderWinTest, RecordsStabilityHistogram) {
  // Record multiple success exits.
  for (size_t i = 0; i < 11; ++i)
    AddProcessExitCode(false, 0);

  // Record a single failure.
  AddProcessExitCode(false, 100);

  WatcherMetricsProviderWin provider(kRegistryPath, test_task_runner_.get());

  provider.ProvideStabilityMetrics(NULL);
  histogram_tester_.ExpectBucketCount(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 0, 11);
  histogram_tester_.ExpectBucketCount(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 100, 1);
  histogram_tester_.ExpectTotalCount(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 12);

  // Verify that the reported values are gone.
  EXPECT_EQ(0u, ExitCodeRegistryPathValueCount());
}

TEST_F(WatcherMetricsProviderWinTest, DoesNotReportOwnProcessId) {
  // Record multiple success exits.
  for (size_t i = 0; i < 11; ++i)
    AddProcessExitCode(i, 0);

  // Record own process as STILL_ACTIVE.
  AddProcessExitCode(true, STILL_ACTIVE);

  WatcherMetricsProviderWin provider(kRegistryPath, test_task_runner_.get());

  provider.ProvideStabilityMetrics(NULL);
  histogram_tester_.ExpectUniqueSample(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 0, 11);

  // Verify that the reported values are gone.
  EXPECT_EQ(1u, ExitCodeRegistryPathValueCount());
}

TEST_F(WatcherMetricsProviderWinTest, DeletesRecordedExitFunnelEvents) {
  // Record an exit funnel and make sure the registry is cleaned up on
  // reporting, without recording any events.
  AddExitFunnelEvent(100, L"One", 1000 * 1000);
  AddExitFunnelEvent(101, L"Two", 1010 * 1000);
  AddExitFunnelEvent(102, L"Three", 990 * 1000);

  base::win::RegistryKeyIterator it(HKEY_CURRENT_USER, kRegistryPath);
  EXPECT_EQ(3u, it.SubkeyCount());

  WatcherMetricsProviderWin provider(kRegistryPath, test_task_runner_.get());

  provider.ProvideStabilityMetrics(NULL);
  // Make sure the exit funnel events are no longer recorded in histograms.
  EXPECT_TRUE(
      histogram_tester_.GetAllSamples("Stability.ExitFunnel.One").empty());
  EXPECT_TRUE(
      histogram_tester_.GetAllSamples("Stability.ExitFunnel.Two").empty());
  EXPECT_TRUE(
      histogram_tester_.GetAllSamples("Stability.ExitFunnel.Three").empty());

  // Make sure the subkeys are deleted on reporting.
  ASSERT_EQ(0u, it.SubkeyCount());
}

TEST_F(WatcherMetricsProviderWinTest, DeletesExitcodeKeyWhenNotReporting) {
  // Test that the registry at kRegistryPath is deleted when reporting is
  // disabled.
  ExitFunnel funnel;

  // Record multiple success exits.
  for (size_t i = 0; i < 11; ++i)
    AddProcessExitCode(false, 0);
  // Record a single failure.
  AddProcessExitCode(false, 100);

  // Record an exit funnel.
  ASSERT_TRUE(funnel.InitImpl(kRegistryPath, 4, base::Time::Now()));

  AddExitFunnelEvent(100, L"One", 1000 * 1000);
  AddExitFunnelEvent(101, L"Two", 1010 * 1000);
  AddExitFunnelEvent(102, L"Three", 990 * 1000);

  // Make like the user is opted out of reporting.
  WatcherMetricsProviderWin provider(kRegistryPath, test_task_runner_.get());
  provider.OnRecordingDisabled();

  base::win::RegKey key;
  {
    // The deletion should be scheduled to the test_task_runner, and not happen
    // immediately.
    ASSERT_EQ(ERROR_SUCCESS,
              key.Open(HKEY_CURRENT_USER, kRegistryPath, KEY_READ));
  }

  // Flush the task(s).
  test_task_runner_->RunPendingTasks();

  // Make sure the subkey for the pseudo process has been deleted on reporting.
  ASSERT_EQ(ERROR_FILE_NOT_FOUND,
            key.Open(HKEY_CURRENT_USER, kRegistryPath, KEY_READ));
}

TEST_F(WatcherMetricsProviderWinTest, DeletesOnly100FunnelsAtATime) {
  // Record 200 distinct exit funnels.
  for (size_t i = 0; i < 200; ++i) {
    AddExitFunnelEvent(i, L"One", 10);
    AddExitFunnelEvent(i, L"Two", 10);
  }

  base::win::RegistryKeyIterator it(HKEY_CURRENT_USER, kRegistryPath);
  EXPECT_EQ(200u, it.SubkeyCount());

  {
    // Make like the user is opted out of reporting.
    WatcherMetricsProviderWin provider(kRegistryPath, test_task_runner_.get());
    provider.OnRecordingDisabled();
    // Flush the task(s).
    test_task_runner_->RunPendingTasks();
  }

  // We expect only 100 of the funnels have been scrubbed.
  EXPECT_EQ(100u, it.SubkeyCount());
}

}  // namespace browser_watcher
