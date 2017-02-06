// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a set of user experience metrics data recorded by
// the MetricsService.  This is the unit of data that is sent to the server.

#ifndef COMPONENTS_METRICS_METRICS_LOG_H_
#define COMPONENTS_METRICS_METRICS_LOG_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
class HistogramSamples;
}

namespace content {
struct WebPluginInfo;
}

namespace variations {
struct ActiveGroupId;
}

namespace metrics {

class MetricsProvider;
class MetricsServiceClient;

class MetricsLog {
 public:
  enum LogType {
    INITIAL_STABILITY_LOG,  // The initial log containing stability stats.
    ONGOING_LOG,            // Subsequent logs in a session.
  };

  // Creates a new metrics log of the specified type.
  // |client_id| is the identifier for this profile on this installation
  // |session_id| is an integer that's incremented on each application launch
  // |client| is used to interact with the embedder.
  // |local_state| is the PrefService that this instance should use.
  // Note: |this| instance does not take ownership of the |client|, but rather
  // stores a weak pointer to it. The caller should ensure that the |client| is
  // valid for the lifetime of this class.
  MetricsLog(const std::string& client_id,
             int session_id,
             LogType log_type,
             MetricsServiceClient* client,
             PrefService* local_state);
  virtual ~MetricsLog();

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Computes the MD5 hash of the given string, and returns the first 8 bytes of
  // the hash.
  static uint64_t Hash(const std::string& value);

  // Get the GMT buildtime for the current binary, expressed in seconds since
  // January 1, 1970 GMT.
  // The value is used to identify when a new build is run, so that previous
  // reliability stats, from other builds, can be abandoned.
  static int64_t GetBuildTime();

  // Convenience function to return the current time at a resolution in seconds.
  // This wraps base::TimeTicks, and hence provides an abstract time that is
  // always incrementing for use in measuring time durations.
  static int64_t GetCurrentTime();

  // Records a user-initiated action.
  void RecordUserAction(const std::string& key);

  // Record any changes in a given histogram for transmission.
  void RecordHistogramDelta(const std::string& histogram_name,
                            const base::HistogramSamples& snapshot);

  // Records the current operating environment, including metrics provided by
  // the specified set of |metrics_providers|.  Takes the list of installed
  // plugins, Google Update statistics, and synthetic trial IDs as parameters
  // because those can't be obtained synchronously from the UI thread.
  // A synthetic trial is one that is set up dynamically by code in Chrome. For
  // example, a pref may be mapped to a synthetic trial such that the group
  // is determined by the pref value.
  void RecordEnvironment(
      const std::vector<MetricsProvider*>& metrics_providers,
      const std::vector<variations::ActiveGroupId>& synthetic_trials,
      int64_t install_date,
      int64_t metrics_reporting_enabled_date);

  // Loads the environment proto that was saved by the last RecordEnvironment()
  // call from prefs and clears the pref value. Returns true on success or false
  // if there was no saved environment in prefs or it could not be decoded.
  bool LoadSavedEnvironmentFromPrefs();

  // Writes application stability metrics, including stability metrics provided
  // by the specified set of |metrics_providers|. The system profile portion of
  // the log must have already been filled in by a call to RecordEnvironment()
  // or LoadSavedEnvironmentFromPrefs().
  // NOTE: Has the side-effect of clearing the stability prefs..
  //
  // If this log is of type INITIAL_STABILITY_LOG, records additional info such
  // as number of incomplete shutdowns as well as extra breakpad and debugger
  // stats.
  void RecordStabilityMetrics(
      const std::vector<MetricsProvider*>& metrics_providers,
      base::TimeDelta incremental_uptime,
      base::TimeDelta uptime);

  // Records general metrics based on the specified |metrics_providers|.
  void RecordGeneralMetrics(
      const std::vector<MetricsProvider*>& metrics_providers);

  // Stop writing to this record and generate the encoded representation.
  // None of the Record* methods can be called after this is called.
  void CloseLog();

  // Fills |encoded_log| with the serialized protobuf representation of the
  // record.  Must only be called after CloseLog() has been called.
  void GetEncodedLog(std::string* encoded_log);

  const base::TimeTicks& creation_time() const {
    return creation_time_;
  }

  int num_events() const {
    return uma_proto_.omnibox_event_size() +
           uma_proto_.user_action_event_size();
  }

  LogType log_type() const { return log_type_; }

 protected:
  // Exposed for the sake of mocking/accessing in test code.

  // Fills |field_trial_ids| with the list of initialized field trials name and
  // group ids.
  virtual void GetFieldTrialIds(
      std::vector<variations::ActiveGroupId>* field_trial_ids) const;

  ChromeUserMetricsExtension* uma_proto() { return &uma_proto_; }

  // Exposed to allow subclass to access to export the uma_proto. Can be used
  // by external components to export logs to Chrome.
  const ChromeUserMetricsExtension* uma_proto() const {
    return &uma_proto_;
  }

 private:
  // Returns true if the environment has already been filled in by a call to
  // RecordEnvironment() or LoadSavedEnvironmentFromPrefs().
  bool HasEnvironment() const;

  // Write the default state of the enable metrics checkbox.
  void WriteMetricsEnableDefault(EnableMetricsDefault metrics_default,
                                 SystemProfileProto* system_profile);

  // Returns true if the stability metrics have already been filled in by a
  // call to RecordStabilityMetrics().
  bool HasStabilityMetrics() const;

  // Within the stability group, write required attributes.
  void WriteRequiredStabilityAttributes(PrefService* pref);

  // Within the stability group, write attributes that need to be updated asap
  // and can't be delayed until the user decides to restart chromium.
  // Delaying these stats would bias metrics away from happy long lived
  // chromium processes (ones that don't crash, and keep on running).
  void WriteRealtimeStabilityAttributes(PrefService* pref,
                                        base::TimeDelta incremental_uptime,
                                        base::TimeDelta uptime);

  // closed_ is true when record has been packed up for sending, and should
  // no longer be written to.  It is only used for sanity checking.
  bool closed_;

  // The type of the log, i.e. initial or ongoing.
  const LogType log_type_;

  // Stores the protocol buffer representation for this log.
  ChromeUserMetricsExtension uma_proto_;

  // Used to interact with the embedder. Weak pointer; must outlive |this|
  // instance.
  MetricsServiceClient* const client_;

  // The time when the current log was created.
  const base::TimeTicks creation_time_;

  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLog);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_H_
