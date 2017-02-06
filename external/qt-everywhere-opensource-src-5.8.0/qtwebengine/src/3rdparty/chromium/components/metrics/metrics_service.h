// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that collects information about the user
// experience in order to help improve future versions of the app.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/data_use_tracker.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_manager.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/variations/synthetic_trials.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class DictionaryValue;
class HistogramSamples;
class PrefService;
}

namespace variations {
struct ActiveGroupId;
}

namespace net {
class URLFetcher;
}

namespace metrics {

class MetricsLogUploader;
class MetricsReportingScheduler;
class MetricsServiceAccessor;
class MetricsServiceClient;
class MetricsStateManager;

// See metrics_service.cc for a detailed description.
class MetricsService : public base::HistogramFlattener {
 public:
  // The execution phase of the browser.
  enum ExecutionPhase {
    UNINITIALIZED_PHASE = 0,
    START_METRICS_RECORDING = 100,
    CREATE_PROFILE = 200,
    STARTUP_TIMEBOMB_ARM = 300,
    THREAD_WATCHER_START = 400,
    MAIN_MESSAGE_LOOP_RUN = 500,
    SHUTDOWN_TIMEBOMB_ARM = 600,
    SHUTDOWN_COMPLETE = 700,
  };

  // Creates the MetricsService with the given |state_manager|, |client|, and
  // |local_state|.  Does not take ownership of the paramaters; instead stores
  // a weak pointer to each. Caller should ensure that the parameters are valid
  // for the lifetime of this class.
  MetricsService(MetricsStateManager* state_manager,
                 MetricsServiceClient* client,
                 PrefService* local_state);
  ~MetricsService() override;

  // Initializes metrics recording state. Updates various bookkeeping values in
  // prefs and sets up the scheduler. This is a separate function rather than
  // being done by the constructor so that field trials could be created before
  // this is run.
  void InitializeMetricsRecordingState();

  // Starts the metrics system, turning on recording and uploading of metrics.
  // Should be called when starting up with metrics enabled, or when metrics
  // are turned on.
  void Start();

  // Starts the metrics system in a special test-only mode. Metrics won't ever
  // be uploaded or persisted in this mode, but metrics will be recorded in
  // memory.
  void StartRecordingForTests();

  // Shuts down the metrics system. Should be called at shutdown, or if metrics
  // are turned off.
  void Stop();

  // Enable/disable transmission of accumulated logs and crash reports (dumps).
  // Calling Start() automatically enables reporting, but sending is
  // asyncronous so this can be called immediately after Start() to prevent
  // any uploading.
  void EnableReporting();
  void DisableReporting();

  // Returns the client ID for this client, or the empty string if metrics
  // recording is not currently running.
  std::string GetClientId();

  // Returns the install date of the application, in seconds since the epoch.
  int64_t GetInstallDate();

  // Returns the date at which the current metrics client ID was created as
  // an int64_t containing seconds since the epoch.
  int64_t GetMetricsReportingEnabledDate();

  // Returns true if the last session exited cleanly.
  bool WasLastShutdownClean() const;

  // Returns the preferred entropy provider used to seed persistent activities
  // based on whether or not metrics reporting will be permitted on this client.
  //
  // If metrics reporting is enabled, this method returns an entropy provider
  // that has a high source of entropy, partially based on the client ID.
  // Otherwise, it returns an entropy provider that is based on a low entropy
  // source.
  std::unique_ptr<const base::FieldTrial::EntropyProvider>
  CreateEntropyProvider();

  // At startup, prefs needs to be called with a list of all the pref names and
  // types we'll be using.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // HistogramFlattener:
  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override;
  void InconsistencyDetected(
      base::HistogramBase::Inconsistency problem) override;
  void UniqueInconsistencyDetected(
      base::HistogramBase::Inconsistency problem) override;
  void InconsistencyDetectedInLoggedCount(int amount) override;

  // This should be called when the application is not idle, i.e. the user seems
  // to be interacting with the application.
  void OnApplicationNotIdle();

  // Invoked when we get a WM_SESSIONEND. This places a value in prefs that is
  // reset when RecordCompletedSessionEnd is invoked.
  void RecordStartOfSessionEnd();

  // This should be called when the application is shutting down. It records
  // that session end was successful.
  void RecordCompletedSessionEnd();

#if defined(OS_ANDROID) || defined(OS_IOS)
  // Called when the application is going into background mode.
  void OnAppEnterBackground();

  // Called when the application is coming out of background mode.
  void OnAppEnterForeground();
#else
  // Set the dirty flag, which will require a later call to LogCleanShutdown().
  void LogNeedForCleanShutdown();
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

  static void SetExecutionPhase(ExecutionPhase execution_phase,
                                PrefService* local_state);

  // Saves in the preferences if the crash report registration was successful.
  // This count is eventually send via UMA logs.
  void RecordBreakpadRegistration(bool success);

  // Saves in the preferences if the browser is running under a debugger.
  // This count is eventually send via UMA logs.
  void RecordBreakpadHasDebugger(bool has_debugger);

  bool recording_active() const;
  bool reporting_active() const;

  // Redundant test to ensure that we are notified of a clean exit.
  // This value should be true when process has completed shutdown.
  static bool UmaMetricsProperlyShutdown();

  // Public accessor that returns the list of synthetic field trials. It must
  // only be used for testing.
  void GetCurrentSyntheticFieldTrialsForTesting(
      std::vector<variations::ActiveGroupId>* synthetic_trials);

  // Adds an observer to be notified when the synthetic trials list changes.
  void AddSyntheticTrialObserver(variations::SyntheticTrialObserver* observer);

  // Removes an existing observer of synthetic trials list changes.
  void RemoveSyntheticTrialObserver(
      variations::SyntheticTrialObserver* observer);

  // Register the specified |provider| to provide additional metrics into the
  // UMA log. Should be called during MetricsService initialization only.
  void RegisterMetricsProvider(std::unique_ptr<MetricsProvider> provider);

  // Check if this install was cloned or imaged from another machine. If a
  // clone is detected, reset the client id and low entropy source. This
  // should not be called more than once.
  void CheckForClonedInstall(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Clears the stability metrics that are saved in local state.
  void ClearSavedStabilityMetrics();

  // Pushes a log that has been generated by an external component.
  void PushExternalLog(const std::string& log);

  // Returns a callback to data use pref updating function which can be called
  // from any thread, but this function should be called on UI thread.
  UpdateUsagePrefCallbackType GetDataUseForwardingCallback();

 protected:
  // Exposed for testing.
  MetricsLogManager* log_manager() { return &log_manager_; }

 private:
  friend class MetricsServiceAccessor;

  // The MetricsService has a lifecycle that is stored as a state.
  // See metrics_service.cc for description of this lifecycle.
  enum State {
    INITIALIZED,          // Constructor was called.
    INIT_TASK_SCHEDULED,  // Waiting for deferred init tasks to finish.
    INIT_TASK_DONE,       // Waiting for timer to send initial log.
    SENDING_LOGS,         // Sending logs an creating new ones when we run out.
  };

  enum ShutdownCleanliness {
    CLEANLY_SHUTDOWN = 0xdeadbeef,
    NEED_TO_SHUTDOWN = ~CLEANLY_SHUTDOWN
  };

  // The current state of recording for the MetricsService. The state is UNSET
  // until set to something else, at which point it remains INACTIVE or ACTIVE
  // for the lifetime of the object.
  enum RecordingState {
    INACTIVE,
    ACTIVE,
    UNSET
  };

  typedef std::vector<variations::SyntheticTrialGroup> SyntheticTrialGroups;

  // Registers a field trial name and group to be used to annotate a UMA report
  // with a particular Chrome configuration state. A UMA report will be
  // annotated with this trial group if and only if all events in the report
  // were created after the trial is registered. Only one group name may be
  // registered at a time for a given trial_name. Only the last group name that
  // is registered for a given trial name will be recorded. The values passed
  // in must not correspond to any real field trial in the code.
  void RegisterSyntheticFieldTrial(
      const variations::SyntheticTrialGroup& trial_group);

  // Calls into the client to initialize some system profile metrics.
  void StartInitTask();

  // Callback that moves the state to INIT_TASK_DONE. When this is called, the
  // state should be INIT_TASK_SCHEDULED.
  void FinishedInitTask();

  void OnUserAction(const std::string& action);

  // Get the amount of uptime since this process started and since the last
  // call to this function.  Also updates the cumulative uptime metric (stored
  // as a pref) for uninstall.  Uptimes are measured using TimeTicks, which
  // guarantees that it is monotonic and does not jump if the user changes
  // his/her clock.  The TimeTicks implementation also makes the clock not
  // count time the computer is suspended.
  void GetUptimes(PrefService* pref,
                  base::TimeDelta* incremental_uptime,
                  base::TimeDelta* uptime);

  // Turns recording on or off.
  // DisableRecording() also forces a persistent save of logging state (if
  // anything has been recorded, or transmitted).
  void EnableRecording();
  void DisableRecording();

  // If in_idle is true, sets idle_since_last_transmission to true.
  // If in_idle is false and idle_since_last_transmission_ is true, sets
  // idle_since_last_transmission to false and starts the timer (provided
  // starting the timer is permitted).
  void HandleIdleSinceLastTransmission(bool in_idle);

  // Set up client ID, session ID, etc.
  void InitializeMetricsState();

  // Notifies providers when a new metrics log is created.
  void NotifyOnDidCreateMetricsLog();

  // Schedule the next save of LocalState information.  This is called
  // automatically by the task that performs each save to schedule the next one.
  void ScheduleNextStateSave();

  // Save the LocalState information immediately. This should not be called by
  // anybody other than the scheduler to avoid doing too many writes. When you
  // make a change, call ScheduleNextStateSave() instead.
  void SaveLocalState();

  // Opens a new log for recording user experience metrics.
  void OpenNewLog();

  // Closes out the current log after adding any last information.
  void CloseCurrentLog();

  // Pushes the text of the current and staged logs into persistent storage.
  // Called when Chrome shuts down.
  void PushPendingLogsToPersistentStorage();

  // Ensures that scheduler is running, assuming the current settings are such
  // that metrics should be reported. If not, this is a no-op.
  void StartSchedulerIfNecessary();

  // Starts the process of uploading metrics data.
  void StartScheduledUpload();

  // Called by the client via a callback when final log info collection is
  // complete.
  void OnFinalLogInfoCollectionDone();

  // If recording is enabled, begins uploading the next completed log from
  // the log manager, staging it if necessary.
  void SendNextLog();

  // Returns true if any of the registered metrics providers have critical
  // stability metrics to report in an initial stability log.
  bool ProvidersHaveInitialStabilityMetrics();

  // Prepares the initial stability log, which is only logged when the previous
  // run of Chrome crashed.  This log contains any stability metrics left over
  // from that previous run, and only these stability metrics.  It uses the
  // system profile from the previous session.  Returns true if a log was
  // created.
  bool PrepareInitialStabilityLog();

  // Prepares the initial metrics log, which includes startup histograms and
  // profiler data, as well as incremental stability-related metrics.
  void PrepareInitialMetricsLog();

  // Uploads the currently staged log (which must be non-null).
  void SendStagedLog();

  // Called after transmission completes (either successfully or with failure).
  void OnLogUploadComplete(int response_code);

  // Reads, increments and then sets the specified integer preference.
  void IncrementPrefValue(const char* path);

  // Reads, increments and then sets the specified long preference that is
  // stored as a string.
  void IncrementLongPrefsValue(const char* path);

  // Records that the browser was shut down cleanly.
  void LogCleanShutdown();

  // Records state that should be periodically saved, like uptime and
  // buffered plugin stability statistics.
  void RecordCurrentState(PrefService* pref);

  // Checks whether events should currently be logged.
  bool ShouldLogEvents();

  // Sets the value of the specified path in prefs and schedules a save.
  void RecordBooleanPrefValue(const char* path, bool value);

  // Notifies observers on a synthetic trial list change.
  void NotifySyntheticTrialObservers();

  // Returns a list of synthetic field trials that are older than |time|.
  void GetSyntheticFieldTrialsOlderThan(
      base::TimeTicks time,
      std::vector<variations::ActiveGroupId>* synthetic_trials);

  // Creates a new MetricsLog instance with the given |log_type|.
  std::unique_ptr<MetricsLog> CreateLog(MetricsLog::LogType log_type);

  // Records the current environment (system profile) in |log|.
  void RecordCurrentEnvironment(MetricsLog* log);

  // Record complete list of histograms into the current log.
  // Called when we close a log.
  void RecordCurrentHistograms();

  // Record complete list of stability histograms into the current log,
  // i.e., histograms with the |kUmaStabilityHistogramFlag| flag set.
  void RecordCurrentStabilityHistograms();

  // Skips staged upload and discards the log. Used in case of unsuccessful
  // upload or intentional sampling of logs.
  void SkipAndDiscardUpload();

  // Manager for the various in-flight logs.
  MetricsLogManager log_manager_;

  // |histogram_snapshot_manager_| prepares histogram deltas for transmission.
  base::HistogramSnapshotManager histogram_snapshot_manager_;

  // Used to manage various metrics reporting state prefs, such as client id,
  // low entropy source and whether metrics reporting is enabled. Weak pointer.
  MetricsStateManager* const state_manager_;

  // Used to interact with the embedder. Weak pointer; must outlive |this|
  // instance.
  MetricsServiceClient* const client_;

  // Registered metrics providers.
  ScopedVector<MetricsProvider> metrics_providers_;

  PrefService* local_state_;

  CleanExitBeacon clean_exit_beacon_;

  base::ActionCallback action_callback_;

  // Indicate whether recording and reporting are currently happening.
  // These should not be set directly, but by calling SetRecording and
  // SetReporting.
  RecordingState recording_state_;
  bool reporting_active_;

  // Indicate whether test mode is enabled, where the initial log should never
  // be cut, and logs are neither persisted nor uploaded.
  bool test_mode_active_;

  // The progression of states made by the browser are recorded in the following
  // state.
  State state_;

  // The initial metrics log, used to record startup metrics (histograms and
  // profiler data). Note that if a crash occurred in the previous session, an
  // initial stability log may be sent before this.
  std::unique_ptr<MetricsLog> initial_metrics_log_;

  // Instance of the helper class for uploading logs.
  std::unique_ptr<MetricsLogUploader> log_uploader_;

  // Whether there is a current log upload in progress.
  bool log_upload_in_progress_;

  // Whether the MetricsService object has received any notifications since
  // the last time a transmission was sent.
  bool idle_since_last_transmission_;

  // A number that identifies the how many times the app has been launched.
  int session_id_;

  // The scheduler for determining when uploads should happen.
  std::unique_ptr<MetricsReportingScheduler> scheduler_;

  // Stores the time of the first call to |GetUptimes()|.
  base::TimeTicks first_updated_time_;

  // Stores the time of the last call to |GetUptimes()|.
  base::TimeTicks last_updated_time_;

  // Field trial groups that map to Chrome configuration states.
  SyntheticTrialGroups synthetic_trial_groups_;

  // List of observers of |synthetic_trial_groups_| changes.
  base::ObserverList<variations::SyntheticTrialObserver>
      synthetic_trial_observer_list_;

  // Execution phase the browser is in.
  static ExecutionPhase execution_phase_;

  // Redundant marker to check that we completed our shutdown, and set the
  // exited-cleanly bit in the prefs.
  static ShutdownCleanliness clean_shutdown_status_;

  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, IsPluginProcess);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest,
                           PermutedEntropyCacheClearedWhenLowEntropyReset);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, RegisterSyntheticTrial);

  // Pointer used for obtaining data use pref updater callback on above layers.
  std::unique_ptr<DataUseTracker> data_use_tracker_;

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as MetricsService.
  base::WeakPtrFactory<MetricsService> self_ptr_factory_;

  // Weak pointers factory used for saving state. All weak pointers managed by
  // this factory are invalidated in ScheduleNextStateSave.
  base::WeakPtrFactory<MetricsService> state_saver_factory_;

  DISALLOW_COPY_AND_ASSIGN(MetricsService);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_H_
