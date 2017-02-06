// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RAPPOR_RAPPOR_SERVICE_H_
#define COMPONENTS_RAPPOR_RAPPOR_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "components/metrics/daily_event.h"
#include "components/rappor/rappor_parameters.h"
#include "components/rappor/sample.h"
#include "components/rappor/sampler.h"

class PrefRegistrySimple;
class PrefService;

namespace net {
class URLRequestContextGetter;
}

namespace rappor {

class LogUploaderInterface;
class RapporMetric;
class RapporReports;

// This class provides an interface for recording samples for rappor metrics,
// and periodically generates and uploads reports based on the collected data.
class RapporService : public base::SupportsWeakPtr<RapporService> {
 public:
  // Constructs a RapporService.
  // Calling code is responsible for ensuring that the lifetime of
  // |pref_service| is longer than the lifetime of RapporService.
  // |is_incognito_callback| will be called to test if incognito mode is active.
  RapporService(PrefService* pref_service,
                const base::Callback<bool(void)> is_incognito_callback);
  virtual ~RapporService();

  // Add an observer for collecting daily metrics.
  void AddDailyObserver(
      std::unique_ptr<metrics::DailyEvent::Observer> observer);

  // Initializes the rappor service, including loading the cohort and secret
  // preferences from disk.
  void Initialize(net::URLRequestContextGetter* context);

  // Updates the settings for metric recording and uploading.
  // The RapporService must be initialized before this method is called.
  // |recording_groups| should be set of flags, e.g.
  //    UMA_RECORDING_GROUP | SAFEBROWSING_RECORDING_GROUP
  // If it contains any enabled groups, periodic reports will be
  // generated and queued for upload.
  // If |may_upload| is true, reports will be uploaded from the queue.
  void Update(int recording_groups, bool may_upload);

  // Constructs a Sample object for the caller to record fields in.
  virtual std::unique_ptr<Sample> CreateSample(RapporType);

  // Records a Sample of rappor metric specified by |metric_name|.
  //
  // TODO(holte): Rename RecordSample to RecordString and then rename this
  // to RecordSample.
  //
  // example:
  // std::unique_ptr<Sample> sample =
  // rappor_service->CreateSample(MY_METRIC_TYPE);
  // sample->SetStringField("Field1", "some string");
  // sample->SetFlagsValue("Field2", SOME|FLAGS);
  // rappor_service->RecordSample("MyMetric", std::move(sample));
  //
  // This will result in a report setting two metrics "MyMetric.Field1" and
  // "MyMetric.Field2", and they will both be generated from the same sample,
  // to allow for correllations to be computed.
  virtual void RecordSampleObj(const std::string& metric_name,
                               std::unique_ptr<Sample> sample);

  // Records a sample of the rappor metric specified by |metric_name|.
  // Creates and initializes the metric, if it doesn't yet exist.
  virtual void RecordSample(const std::string& metric_name,
                            RapporType type,
                            const std::string& sample);

  // Registers the names of all of the preferences used by RapporService in the
  // provided PrefRegistry. This should be called before calling Start().
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // Initializes the state of the RapporService.
  void InitializeInternal(std::unique_ptr<LogUploaderInterface> uploader,
                          int32_t cohort,
                          const std::string& secret);

  // Cancels the next call to OnLogInterval.
  virtual void CancelNextLogRotation();

  // Schedules the next call to OnLogInterval.
  virtual void ScheduleNextLogRotation(base::TimeDelta interval);

  // Logs all of the collected metrics to the reports proto message and clears
  // the internal map. Exposed for tests. Returns true if any metrics were
  // recorded.
  bool ExportMetrics(RapporReports* reports);

 private:
  // Records a sample of the rappor metric specified by |parameters|.
  // Creates and initializes the metric, if it doesn't yet exist.
  // Exposed for tests.
  void RecordSampleInternal(const std::string& metric_name,
                            const RapporParameters& parameters,
                            const std::string& sample);

  // Checks if the service has been started successfully.
  bool IsInitialized() const;

  // Called whenever the logging interval elapses to generate a new log of
  // reports and pass it to the uploader.
  void OnLogInterval();

  // Check if recording of the metric is allowed, given it's parameters.
  // This will check that we are not in incognito mode, and that the
  // appropriate recording group is enabled.
  bool RecordingAllowed(const RapporParameters& parameters);

  // Finds a metric in the metrics_map_, creating it if it doesn't already
  // exist.
  RapporMetric* LookUpMetric(const std::string& metric_name,
                             const RapporParameters& parameters);

  // A weak pointer to the PrefService used to read and write preferences.
  PrefService* pref_service_;

  // A callback for testing if incognito mode is active;
  const base::Callback<bool(void)> is_incognito_callback_;

  // Client-side secret used to generate fake bits.
  std::string secret_;

  // The cohort this client is assigned to. -1 is uninitialized.
  int32_t cohort_;

  // Timer which schedules calls to OnLogInterval().
  base::OneShotTimer log_rotation_timer_;

  // A daily event for collecting metrics once a day.
  metrics::DailyEvent daily_event_;

  // A private LogUploader instance for sending reports to the server.
  std::unique_ptr<LogUploaderInterface> uploader_;

  // The set of recording groups that metrics are being recorded, e.g.
  //     UMA_RECORDING_GROUP | SAFEBROWSING_RECORDING_GROUP
  int recording_groups_;

  // We keep all registered metrics in a map, from name to metric.
  // The map owns the metrics it contains.
  std::map<std::string, RapporMetric*> metrics_map_;

  internal::Sampler sampler_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(RapporService);
};

}  // namespace rappor

#endif  // COMPONENTS_RAPPOR_RAPPOR_SERVICE_H_
