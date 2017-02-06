// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_METRICS_CAST_METRICS_HELPER_H_
#define CHROMECAST_BASE_METRICS_CAST_METRICS_HELPER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {
namespace metrics {

// Helper class for tracking complex metrics. This particularly includes
// playback metrics that span events across time, such as "time from app launch
// to video being rendered."
class CastMetricsHelper {
 public:
  enum BufferingType {
    kInitialBuffering,
    kBufferingAfterUnderrun,
    kAbortedBuffering,
  };

  typedef base::Callback<void(const std::string&)> RecordActionCallback;

  class MetricsSink {
   public:
    virtual ~MetricsSink() {}

    virtual void OnAction(const std::string& action) = 0;
    virtual void OnEnumerationEvent(const std::string& name,
                                    int value, int num_buckets) = 0;
    virtual void OnTimeEvent(const std::string& name,
                             const base::TimeDelta& value,
                             const base::TimeDelta& min,
                             const base::TimeDelta& max,
                             int num_buckets) = 0;
  };

  // Decodes action_name/app_id/session_id/sdk_version from metrics name.
  // Return false if the metrics name is not generated from
  // EncodeAppInfoIntoMetricsName() with correct format.
  static bool DecodeAppInfoFromMetricsName(
      const std::string& metrics_name,
      std::string* action_name,
      std::string* app_id,
      std::string* session_id,
      std::string* sdk_version);

  static CastMetricsHelper* GetInstance();

  explicit CastMetricsHelper(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~CastMetricsHelper();

  // This function updates the info and stores the startup time of the current
  // active application
  virtual void UpdateCurrentAppInfo(const std::string& app_id,
                                    const std::string& session_id);
  // This function updates the sdk version of the current active application
  virtual void UpdateSDKInfo(const std::string& sdk_version);

  // Logs UMA record for media play/pause user actions.
  virtual void LogMediaPlay();
  virtual void LogMediaPause();

  // Logs a simple UMA user action.
  // This is used as an in-place replacement of content::RecordComputedAction().
  virtual void RecordSimpleAction(const std::string& action);

  // Logs a generic event.
  virtual void RecordEventWithValue(const std::string& action, int value);

  // Logs application specific events.
  virtual void RecordApplicationEvent(const std::string& event);
  virtual void RecordApplicationEventWithValue(const std::string& event,
                                               int value);

  // Logs UMA record of the time the app made its first paint.
  virtual void LogTimeToFirstPaint();

  // Logs UMA record of the time the app pushed its first audio frame.
  virtual void LogTimeToFirstAudio();

  // Logs UMA record of the time needed to re-buffer A/V.
  virtual void LogTimeToBufferAv(BufferingType buffering_type,
                                 base::TimeDelta time);

  // Returns metrics name with app name between prefix and suffix.
  virtual std::string GetMetricsNameWithAppName(
      const std::string& prefix,
      const std::string& suffix) const;

  // Provides a MetricsSink instance to delegate UMA event logging.
  // Once the delegate interface is set, CastMetricsHelper will not log UMA
  // events internally unless SetMetricsSink(NULL) is called.
  // CastMetricsHelper can only hold one MetricsSink instance.
  // Caller retains ownership of MetricsSink.
  virtual void SetMetricsSink(MetricsSink* delegate);

  // Sets a default callback to record user action when MetricsSink is not set.
  // This function could be called multiple times (in unittests), and
  // CastMetricsHelper only honors the last one.
  virtual void SetRecordActionCallback(const RecordActionCallback& callback);

  // Sets an all-0's session ID for running browser tests.
  void SetDummySessionIdForTesting();

 protected:
  // Creates a CastMetricsHelper instance with no task runner. This should only
  // be used by tests, since invoking any non-overridden methods on this
  // instance will cause a failure.
  CastMetricsHelper();

 private:
  static std::string EncodeAppInfoIntoMetricsName(
      const std::string& action_name,
      const std::string& app_id,
      const std::string& session_id,
      const std::string& sdk_version);

  void LogEnumerationHistogramEvent(const std::string& name,
                                    int value, int num_buckets);
  void LogTimeHistogramEvent(const std::string& name,
                             const base::TimeDelta& value,
                             const base::TimeDelta& min,
                             const base::TimeDelta& max,
                             int num_buckets);
  void LogMediumTimeHistogramEvent(const std::string& name,
                                   const base::TimeDelta& value);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Start time of the most recent app.
  base::TimeTicks app_start_time_;

  // Currently running app id. Used to construct histogram name.
  std::string app_id_;
  std::string session_id_;
  std::string sdk_version_;

  MetricsSink* metrics_sink_;

  bool logged_first_audio_;

  // Default RecordAction callback when metrics_sink_ is not set.
  RecordActionCallback record_action_callback_;

  DISALLOW_COPY_AND_ASSIGN(CastMetricsHelper);
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BASE_METRICS_CAST_METRICS_HELPER_H_
