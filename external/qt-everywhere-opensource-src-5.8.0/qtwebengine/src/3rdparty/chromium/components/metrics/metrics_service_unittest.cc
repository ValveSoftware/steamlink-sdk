// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/platform_thread.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test_enabled_state_provider.h"
#include "components/metrics/test_metrics_provider.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/metrics_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {

namespace {

void StoreNoClientInfoBackup(const ClientInfo& /* client_info */) {
}

std::unique_ptr<ClientInfo> ReturnNoBackup() {
  return std::unique_ptr<ClientInfo>();
}

class TestMetricsService : public MetricsService {
 public:
  TestMetricsService(MetricsStateManager* state_manager,
                     MetricsServiceClient* client,
                     PrefService* local_state)
      : MetricsService(state_manager, client, local_state) {}
  ~TestMetricsService() override {}

  using MetricsService::log_manager;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMetricsService);
};

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id,
                 int session_id,
                 MetricsServiceClient* client,
                 PrefService* local_state)
      : MetricsLog(client_id,
                   session_id,
                   MetricsLog::ONGOING_LOG,
                   client,
                   local_state) {}

  ~TestMetricsLog() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMetricsLog);
};

class MetricsServiceTest : public testing::Test {
 public:
  MetricsServiceTest()
      : enabled_state_provider_(new TestEnabledStateProvider(false, false)) {
    base::SetRecordActionTaskRunner(message_loop.task_runner());
    MetricsService::RegisterPrefs(testing_local_state_.registry());
    metrics_state_manager_ = MetricsStateManager::Create(
        GetLocalState(), enabled_state_provider_.get(),
        base::Bind(&StoreNoClientInfoBackup), base::Bind(&ReturnNoBackup));
  }

  ~MetricsServiceTest() override {
    MetricsService::SetExecutionPhase(MetricsService::UNINITIALIZED_PHASE,
                                      GetLocalState());
  }

  MetricsStateManager* GetMetricsStateManager() {
    return metrics_state_manager_.get();
  }

  PrefService* GetLocalState() { return &testing_local_state_; }

  // Sets metrics reporting as enabled for testing.
  void EnableMetricsReporting() {
    enabled_state_provider_->set_consent(true);
    enabled_state_provider_->set_enabled(true);
  }

  // Waits until base::TimeTicks::Now() no longer equals |value|. This should
  // take between 1-15ms per the documented resolution of base::TimeTicks.
  void WaitUntilTimeChanges(const base::TimeTicks& value) {
    while (base::TimeTicks::Now() == value) {
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
    }
  }

  // Returns true if there is a synthetic trial in the given vector that matches
  // the given trial name and trial group; returns false otherwise.
  bool HasSyntheticTrial(
      const std::vector<variations::ActiveGroupId>& synthetic_trials,
      const std::string& trial_name,
      const std::string& trial_group) {
    uint32_t trial_name_hash = HashName(trial_name);
    uint32_t trial_group_hash = HashName(trial_group);
    for (const variations::ActiveGroupId& trial : synthetic_trials) {
      if (trial.name == trial_name_hash && trial.group == trial_group_hash)
        return true;
    }
    return false;
  }

  // Finds a histogram with the specified |name_hash| in |histograms|.
  const base::HistogramBase* FindHistogram(
      const base::StatisticsRecorder::Histograms& histograms,
      uint64_t name_hash) {
    for (const base::HistogramBase* histogram : histograms) {
      if (name_hash == base::HashMetricName(histogram->histogram_name()))
        return histogram;
    }
    return nullptr;
  }

  // Checks whether |uma_log| contains any histograms that are not flagged
  // with kUmaStabilityHistogramFlag. Stability logs should only contain such
  // histograms.
  void CheckForNonStabilityHistograms(
      const ChromeUserMetricsExtension& uma_log) {
    const int kStabilityFlags = base::HistogramBase::kUmaStabilityHistogramFlag;
    base::StatisticsRecorder::Histograms histograms;
    base::StatisticsRecorder::GetHistograms(&histograms);
    for (int i = 0; i < uma_log.histogram_event_size(); ++i) {
      const uint64_t hash = uma_log.histogram_event(i).name_hash();

      const base::HistogramBase* histogram = FindHistogram(histograms, hash);
      EXPECT_TRUE(histogram) << hash;

      EXPECT_EQ(kStabilityFlags, histogram->flags() & kStabilityFlags) << hash;
    }
  }

 private:
  std::unique_ptr<TestEnabledStateProvider> enabled_state_provider_;
  TestingPrefServiceSimple testing_local_state_;
  std::unique_ptr<MetricsStateManager> metrics_state_manager_;
  base::MessageLoop message_loop;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServiceTest);
};

}  // namespace

TEST_F(MetricsServiceTest, InitialStabilityLogAfterCleanShutDown) {
  EnableMetricsReporting();
  GetLocalState()->SetBoolean(prefs::kStabilityExitedCleanly, true);

  TestMetricsServiceClient client;
  TestMetricsService service(
      GetMetricsStateManager(), &client, GetLocalState());

  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();

  // No initial stability log should be generated.
  EXPECT_FALSE(service.log_manager()->has_unsent_logs());
  EXPECT_FALSE(service.log_manager()->has_staged_log());

  // Ensure that HasInitialStabilityMetrics() is always called on providers,
  // for consistency, even if other conditions already indicate their presence.
  EXPECT_TRUE(test_provider->has_initial_stability_metrics_called());

  // The test provider should not have been called upon to provide initial
  // stability nor regular stability metrics.
  EXPECT_FALSE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_FALSE(test_provider->provide_stability_metrics_called());
}

TEST_F(MetricsServiceTest, InitialStabilityLogAtProviderRequest) {
  EnableMetricsReporting();

  // Save an existing system profile to prefs, to correspond to what would be
  // saved from a previous session.
  TestMetricsServiceClient client;
  TestMetricsLog log("client", 1, &client, GetLocalState());
  log.RecordEnvironment(std::vector<MetricsProvider*>(),
                        std::vector<variations::ActiveGroupId>(), 0, 0);

  // Record stability build time and version from previous session, so that
  // stability metrics (including exited cleanly flag) won't be cleared.
  GetLocalState()->SetInt64(prefs::kStabilityStatsBuildTime,
                            MetricsLog::GetBuildTime());
  GetLocalState()->SetString(prefs::kStabilityStatsVersion,
                             client.GetVersionString());

  // Set the clean exit flag, as that will otherwise cause a stabilty
  // log to be produced, irrespective provider requests.
  GetLocalState()->SetBoolean(prefs::kStabilityExitedCleanly, true);

  TestMetricsService service(
      GetMetricsStateManager(), &client, GetLocalState());
  // Add a metrics provider that requests a stability log.
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  test_provider->set_has_initial_stability_metrics(true);
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();

  // The initial stability log should be generated and persisted in unsent logs.
  MetricsLogManager* log_manager = service.log_manager();
  EXPECT_TRUE(log_manager->has_unsent_logs());
  EXPECT_FALSE(log_manager->has_staged_log());

  // Ensure that HasInitialStabilityMetrics() is always called on providers,
  // for consistency, even if other conditions already indicate their presence.
  EXPECT_TRUE(test_provider->has_initial_stability_metrics_called());

  // The test provider should have been called upon to provide initial
  // stability and regular stability metrics.
  EXPECT_TRUE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());

  // Stage the log and retrieve it.
  log_manager->StageNextLogForUpload();
  EXPECT_TRUE(log_manager->has_staged_log());

  std::string uncompressed_log;
  EXPECT_TRUE(compression::GzipUncompress(log_manager->staged_log(),
                                          &uncompressed_log));

  ChromeUserMetricsExtension uma_log;
  EXPECT_TRUE(uma_log.ParseFromString(uncompressed_log));

  EXPECT_TRUE(uma_log.has_client_id());
  EXPECT_TRUE(uma_log.has_session_id());
  EXPECT_TRUE(uma_log.has_system_profile());
  EXPECT_EQ(0, uma_log.user_action_event_size());
  EXPECT_EQ(0, uma_log.omnibox_event_size());
  EXPECT_EQ(0, uma_log.profiler_event_size());
  EXPECT_EQ(0, uma_log.perf_data_size());
  CheckForNonStabilityHistograms(uma_log);

  // As there wasn't an unclean shutdown, this log has zero crash count.
  EXPECT_EQ(0, uma_log.system_profile().stability().crash_count());
}

TEST_F(MetricsServiceTest, InitialStabilityLogAfterCrash) {
  EnableMetricsReporting();
  GetLocalState()->ClearPref(prefs::kStabilityExitedCleanly);

  // Set up prefs to simulate restarting after a crash.

  // Save an existing system profile to prefs, to correspond to what would be
  // saved from a previous session.
  TestMetricsServiceClient client;
  TestMetricsLog log("client", 1, &client, GetLocalState());
  log.RecordEnvironment(std::vector<MetricsProvider*>(),
                        std::vector<variations::ActiveGroupId>(), 0, 0);

  // Record stability build time and version from previous session, so that
  // stability metrics (including exited cleanly flag) won't be cleared.
  GetLocalState()->SetInt64(prefs::kStabilityStatsBuildTime,
                            MetricsLog::GetBuildTime());
  GetLocalState()->SetString(prefs::kStabilityStatsVersion,
                             client.GetVersionString());

  GetLocalState()->SetBoolean(prefs::kStabilityExitedCleanly, false);

  TestMetricsService service(
      GetMetricsStateManager(), &client, GetLocalState());
  // Add a provider.
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));
  service.InitializeMetricsRecordingState();

  // The initial stability log should be generated and persisted in unsent logs.
  MetricsLogManager* log_manager = service.log_manager();
  EXPECT_TRUE(log_manager->has_unsent_logs());
  EXPECT_FALSE(log_manager->has_staged_log());

  // Ensure that HasInitialStabilityMetrics() is always called on providers,
  // for consistency, even if other conditions already indicate their presence.
  EXPECT_TRUE(test_provider->has_initial_stability_metrics_called());

  // The test provider should have been called upon to provide initial
  // stability and regular stability metrics.
  EXPECT_TRUE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());

  // Stage the log and retrieve it.
  log_manager->StageNextLogForUpload();
  EXPECT_TRUE(log_manager->has_staged_log());

  std::string uncompressed_log;
  EXPECT_TRUE(compression::GzipUncompress(log_manager->staged_log(),
                                          &uncompressed_log));

  ChromeUserMetricsExtension uma_log;
  EXPECT_TRUE(uma_log.ParseFromString(uncompressed_log));

  EXPECT_TRUE(uma_log.has_client_id());
  EXPECT_TRUE(uma_log.has_session_id());
  EXPECT_TRUE(uma_log.has_system_profile());
  EXPECT_EQ(0, uma_log.user_action_event_size());
  EXPECT_EQ(0, uma_log.omnibox_event_size());
  EXPECT_EQ(0, uma_log.profiler_event_size());
  EXPECT_EQ(0, uma_log.perf_data_size());
  CheckForNonStabilityHistograms(uma_log);

  EXPECT_EQ(1, uma_log.system_profile().stability().crash_count());
}

TEST_F(MetricsServiceTest, RegisterSyntheticTrial) {
  TestMetricsServiceClient client;
  MetricsService service(GetMetricsStateManager(), &client, GetLocalState());

  // Add two synthetic trials and confirm that they show up in the list.
  variations::SyntheticTrialGroup trial1(HashName("TestTrial1"),
                                         HashName("Group1"));
  service.RegisterSyntheticFieldTrial(trial1);

  variations::SyntheticTrialGroup trial2(HashName("TestTrial2"),
                                         HashName("Group2"));
  service.RegisterSyntheticFieldTrial(trial2);
  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(base::TimeTicks::Now());

  service.log_manager_.BeginLoggingWithLog(std::unique_ptr<MetricsLog>(
      new MetricsLog("clientID", 1, MetricsLog::INITIAL_STABILITY_LOG, &client,
                     GetLocalState())));
  // Save the time when the log was started (it's okay for this to be greater
  // than the time recorded by the above call since it's used to ensure the
  // value changes).
  const base::TimeTicks begin_log_time = base::TimeTicks::Now();

  std::vector<variations::ActiveGroupId> synthetic_trials;
  service.GetSyntheticFieldTrialsOlderThan(base::TimeTicks::Now(),
                                           &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group1"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(begin_log_time);

  // Change the group for the first trial after the log started.
  variations::SyntheticTrialGroup trial3(HashName("TestTrial1"),
                                         HashName("Group2"));
  service.RegisterSyntheticFieldTrial(trial3);
  service.GetSyntheticFieldTrialsOlderThan(begin_log_time, &synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Add a new trial after the log started and confirm that it doesn't show up.
  variations::SyntheticTrialGroup trial4(HashName("TestTrial3"),
                                         HashName("Group3"));
  service.RegisterSyntheticFieldTrial(trial4);
  service.GetSyntheticFieldTrialsOlderThan(begin_log_time, &synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(base::TimeTicks::Now());

  // Start a new log and ensure all three trials appear in it.
  service.log_manager_.FinishCurrentLog();
  service.log_manager_.BeginLoggingWithLog(
      std::unique_ptr<MetricsLog>(new MetricsLog(
          "clientID", 1, MetricsLog::ONGOING_LOG, &client, GetLocalState())));
  service.GetSyntheticFieldTrialsOlderThan(
      service.log_manager_.current_log()->creation_time(), &synthetic_trials);
  EXPECT_EQ(3U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial3", "Group3"));
  service.log_manager_.FinishCurrentLog();
}

TEST_F(MetricsServiceTest,
       MetricsProviderOnRecordingDisabledCalledOnInitialStop) {
  TestMetricsServiceClient client;
  TestMetricsService service(
      GetMetricsStateManager(), &client, GetLocalState());

  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();
  service.Stop();

  EXPECT_TRUE(test_provider->on_recording_disabled_called());
}

TEST_F(MetricsServiceTest, MetricsProvidersInitialized) {
  TestMetricsServiceClient client;
  TestMetricsService service(
      GetMetricsStateManager(), &client, GetLocalState());

  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();

  EXPECT_TRUE(test_provider->init_called());
}

}  // namespace metrics
