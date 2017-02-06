// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log_manager.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

// Dummy serializer that just stores logs in memory.
class TestLogPrefService : public TestingPrefServiceSimple {
 public:
  TestLogPrefService() {
    registry()->RegisterListPref(prefs::kMetricsInitialLogs);
    registry()->RegisterListPref(prefs::kMetricsOngoingLogs);
  }

  // Returns the number of logs of the given type.
  size_t TypeCount(MetricsLog::LogType log_type) {
    int list_length = 0;
    if (log_type == MetricsLog::INITIAL_STABILITY_LOG)
      list_length = GetList(prefs::kMetricsInitialLogs)->GetSize();
    else
      list_length = GetList(prefs::kMetricsOngoingLogs)->GetSize();
    return list_length / 2;
  }
};

}  // namespace

TEST(MetricsLogManagerTest, StandardFlow) {
  TestMetricsServiceClient client;
  TestLogPrefService pref_service;
  MetricsLogManager log_manager(&pref_service, 0);

  // Make sure a new manager has a clean slate.
  EXPECT_EQ(NULL, log_manager.current_log());
  EXPECT_FALSE(log_manager.has_staged_log());
  EXPECT_FALSE(log_manager.has_unsent_logs());

  // Check that the normal flow works.
  MetricsLog* initial_log = new MetricsLog(
      "id", 0, MetricsLog::INITIAL_STABILITY_LOG, &client, &pref_service);
  log_manager.BeginLoggingWithLog(base::WrapUnique(initial_log));
  EXPECT_EQ(initial_log, log_manager.current_log());
  EXPECT_FALSE(log_manager.has_staged_log());

  log_manager.FinishCurrentLog();
  EXPECT_EQ(NULL, log_manager.current_log());
  EXPECT_TRUE(log_manager.has_unsent_logs());
  EXPECT_FALSE(log_manager.has_staged_log());

  MetricsLog* second_log =
      new MetricsLog("id", 0, MetricsLog::ONGOING_LOG, &client, &pref_service);
  log_manager.BeginLoggingWithLog(base::WrapUnique(second_log));
  EXPECT_EQ(second_log, log_manager.current_log());

  log_manager.StageNextLogForUpload();
  EXPECT_TRUE(log_manager.has_staged_log());
  EXPECT_FALSE(log_manager.staged_log().empty());

  log_manager.DiscardStagedLog();
  EXPECT_EQ(second_log, log_manager.current_log());
  EXPECT_FALSE(log_manager.has_staged_log());
  EXPECT_FALSE(log_manager.has_unsent_logs());

  EXPECT_FALSE(log_manager.has_unsent_logs());
}

TEST(MetricsLogManagerTest, AbandonedLog) {
  TestMetricsServiceClient client;
  TestLogPrefService pref_service;
  MetricsLogManager log_manager(&pref_service, 0);

  MetricsLog* dummy_log = new MetricsLog(
      "id", 0, MetricsLog::INITIAL_STABILITY_LOG, &client, &pref_service);
  log_manager.BeginLoggingWithLog(base::WrapUnique(dummy_log));
  EXPECT_EQ(dummy_log, log_manager.current_log());

  log_manager.DiscardCurrentLog();
  EXPECT_EQ(NULL, log_manager.current_log());
  EXPECT_FALSE(log_manager.has_staged_log());
}

TEST(MetricsLogManagerTest, InterjectedLog) {
  TestMetricsServiceClient client;
  TestLogPrefService pref_service;
  MetricsLogManager log_manager(&pref_service, 0);

  MetricsLog* ongoing_log =
      new MetricsLog("id", 0, MetricsLog::ONGOING_LOG, &client, &pref_service);
  MetricsLog* temp_log = new MetricsLog(
      "id", 0, MetricsLog::INITIAL_STABILITY_LOG, &client, &pref_service);

  log_manager.BeginLoggingWithLog(base::WrapUnique(ongoing_log));
  EXPECT_EQ(ongoing_log, log_manager.current_log());

  log_manager.PauseCurrentLog();
  EXPECT_EQ(NULL, log_manager.current_log());

  log_manager.BeginLoggingWithLog(base::WrapUnique(temp_log));
  EXPECT_EQ(temp_log, log_manager.current_log());
  log_manager.FinishCurrentLog();
  EXPECT_EQ(NULL, log_manager.current_log());

  log_manager.ResumePausedLog();
  EXPECT_EQ(ongoing_log, log_manager.current_log());

  EXPECT_FALSE(log_manager.has_staged_log());
  log_manager.StageNextLogForUpload();
  log_manager.DiscardStagedLog();
  EXPECT_FALSE(log_manager.has_unsent_logs());
}

TEST(MetricsLogManagerTest, InterjectedLogPreservesType) {
  TestMetricsServiceClient client;
  TestLogPrefService pref_service;
  MetricsLogManager log_manager(&pref_service, 0);
  log_manager.LoadPersistedUnsentLogs();

  log_manager.BeginLoggingWithLog(base::WrapUnique(new MetricsLog(
      "id", 0, MetricsLog::ONGOING_LOG, &client, &pref_service)));
  log_manager.PauseCurrentLog();
  log_manager.BeginLoggingWithLog(base::WrapUnique(new MetricsLog(
      "id", 0, MetricsLog::INITIAL_STABILITY_LOG, &client, &pref_service)));
  log_manager.FinishCurrentLog();
  log_manager.ResumePausedLog();
  log_manager.StageNextLogForUpload();
  log_manager.DiscardStagedLog();

  // Verify that the remaining log (which is the original ongoing log) still
  // has the right type.
  log_manager.FinishCurrentLog();
  log_manager.PersistUnsentLogs();
  EXPECT_EQ(0U, pref_service.TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
  EXPECT_EQ(1U, pref_service.TypeCount(MetricsLog::ONGOING_LOG));
}

TEST(MetricsLogManagerTest, StoreAndLoad) {
  TestMetricsServiceClient client;
  TestLogPrefService pref_service;
  // Set up some in-progress logging in a scoped log manager simulating the
  // leadup to quitting, then persist as would be done on quit.
  {
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();

    // Simulate a log having already been unsent from a previous session.
    {
      std::string log("proto");
      PersistedLogs ongoing_logs(&pref_service, prefs::kMetricsOngoingLogs, 1,
                                 1, 0);
      ongoing_logs.StoreLog(log);
      ongoing_logs.SerializeLogs();
    }
    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLog::ONGOING_LOG));
    EXPECT_FALSE(log_manager.has_unsent_logs());
    log_manager.LoadPersistedUnsentLogs();
    EXPECT_TRUE(log_manager.has_unsent_logs());

    log_manager.BeginLoggingWithLog(base::WrapUnique(new MetricsLog(
        "id", 0, MetricsLog::INITIAL_STABILITY_LOG, &client, &pref_service)));
    log_manager.FinishCurrentLog();
    log_manager.BeginLoggingWithLog(base::WrapUnique(new MetricsLog(
        "id", 0, MetricsLog::ONGOING_LOG, &client, &pref_service)));
    log_manager.StageNextLogForUpload();
    log_manager.FinishCurrentLog();

    // Nothing should be written out until PersistUnsentLogs is called.
    EXPECT_EQ(0U, pref_service.TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLog::ONGOING_LOG));
    log_manager.PersistUnsentLogs();
    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(2U, pref_service.TypeCount(MetricsLog::ONGOING_LOG));
  }

  // Now simulate the relaunch, ensure that the log manager restores
  // everything correctly, and verify that once the are handled they are not
  // re-persisted.
  {
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();
    EXPECT_TRUE(log_manager.has_unsent_logs());

    log_manager.StageNextLogForUpload();
    log_manager.DiscardStagedLog();
    // The initial log should be sent first; update the persisted storage to
    // verify.
    log_manager.PersistUnsentLogs();
    EXPECT_EQ(0U, pref_service.TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(2U, pref_service.TypeCount(MetricsLog::ONGOING_LOG));

    // Handle the first ongoing log.
    log_manager.StageNextLogForUpload();
    log_manager.DiscardStagedLog();
    EXPECT_TRUE(log_manager.has_unsent_logs());

    // Handle the last log.
    log_manager.StageNextLogForUpload();
    log_manager.DiscardStagedLog();
    EXPECT_FALSE(log_manager.has_unsent_logs());

    // Nothing should have changed "on disk" since PersistUnsentLogs hasn't been
    // called again.
    EXPECT_EQ(2U, pref_service.TypeCount(MetricsLog::ONGOING_LOG));
    // Persist, and make sure nothing is left.
    log_manager.PersistUnsentLogs();
    EXPECT_EQ(0U, pref_service.TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(0U, pref_service.TypeCount(MetricsLog::ONGOING_LOG));
  }
}

TEST(MetricsLogManagerTest, StoreStagedLogTypes) {
  TestMetricsServiceClient client;

  // Ensure that types are preserved when storing staged logs.
  {
    TestLogPrefService pref_service;
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();

    log_manager.BeginLoggingWithLog(base::WrapUnique(new MetricsLog(
        "id", 0, MetricsLog::ONGOING_LOG, &client, &pref_service)));
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.PersistUnsentLogs();

    EXPECT_EQ(0U, pref_service.TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLog::ONGOING_LOG));
  }

  {
    TestLogPrefService pref_service;
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();

    log_manager.BeginLoggingWithLog(base::WrapUnique(new MetricsLog(
        "id", 0, MetricsLog::INITIAL_STABILITY_LOG, &client, &pref_service)));
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.PersistUnsentLogs();

    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(0U, pref_service.TypeCount(MetricsLog::ONGOING_LOG));
  }
}

TEST(MetricsLogManagerTest, LargeLogDiscarding) {
  TestMetricsServiceClient client;
  TestLogPrefService pref_service;
  // Set the size threshold very low, to verify that it's honored.
  MetricsLogManager log_manager(&pref_service, 1);
  log_manager.LoadPersistedUnsentLogs();

  log_manager.BeginLoggingWithLog(base::WrapUnique(new MetricsLog(
      "id", 0, MetricsLog::INITIAL_STABILITY_LOG, &client, &pref_service)));
  log_manager.FinishCurrentLog();
  log_manager.BeginLoggingWithLog(base::WrapUnique(new MetricsLog(
      "id", 0, MetricsLog::ONGOING_LOG, &client, &pref_service)));
  log_manager.FinishCurrentLog();

  // Only the ongoing log should be written out, due to the threshold.
  log_manager.PersistUnsentLogs();
  EXPECT_EQ(1U, pref_service.TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
  EXPECT_EQ(0U, pref_service.TypeCount(MetricsLog::ONGOING_LOG));
}

TEST(MetricsLogManagerTest, DiscardOrder) {
  // Ensure that the correct log is discarded if new logs are pushed while
  // a log is staged.
  TestMetricsServiceClient client;
  {
    TestLogPrefService pref_service;
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();

    log_manager.BeginLoggingWithLog(base::WrapUnique(new MetricsLog(
        "id", 0, MetricsLog::INITIAL_STABILITY_LOG, &client, &pref_service)));
    log_manager.FinishCurrentLog();
    log_manager.BeginLoggingWithLog(base::WrapUnique(new MetricsLog(
        "id", 0, MetricsLog::ONGOING_LOG, &client, &pref_service)));
    log_manager.StageNextLogForUpload();
    log_manager.FinishCurrentLog();
    log_manager.DiscardStagedLog();

    log_manager.PersistUnsentLogs();
    EXPECT_EQ(0U, pref_service.TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLog::ONGOING_LOG));
  }
}

}  // namespace metrics
