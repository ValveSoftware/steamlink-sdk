// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/metrics/cast_metrics_helper.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromecast/base/metrics/cast_histograms.h"
#include "chromecast/base/metrics/grouped_histogram.h"

namespace chromecast {
namespace metrics {

// A useful macro to make sure current member function runs on the valid thread.
#define MAKE_SURE_THREAD(callback, ...)                                        \
  if (!task_runner_->BelongsToCurrentThread()) {                               \
    task_runner_->PostTask(FROM_HERE,                                          \
                           base::Bind(&CastMetricsHelper::callback,            \
                                      base::Unretained(this), ##__VA_ARGS__)); \
    return;                                                                    \
  }

namespace {

CastMetricsHelper* g_instance = NULL;

const char kMetricsNameAppInfoDelimiter = '#';

std::unique_ptr<std::string> SerializeToJson(const base::Value& value) {
  std::unique_ptr<std::string> json_str(new std::string());
  JSONStringValueSerializer serializer(json_str.get());
  if (!serializer.Serialize(value))
    json_str.reset(nullptr);
  return json_str;
}

std::unique_ptr<base::DictionaryValue> CreateEventBase(
    const std::string& name) {
  std::unique_ptr<base::DictionaryValue> cast_event(
      new base::DictionaryValue());
  cast_event->SetString("name", name);
  cast_event->SetDouble("time", base::TimeTicks::Now().ToInternalValue());

  return cast_event;
}

}  // namespace

// static

// NOTE(gfhuang): This is a hacky way to encode/decode app infos into a
// string. Mainly because it's hard to add another metrics serialization type
// into components/metrics/serialization/.
// static
bool CastMetricsHelper::DecodeAppInfoFromMetricsName(
    const std::string& metrics_name,
    std::string* action_name,
    std::string* app_id,
    std::string* session_id,
    std::string* sdk_version) {
  DCHECK(action_name);
  DCHECK(app_id);
  DCHECK(session_id);
  DCHECK(sdk_version);
  if (metrics_name.find(kMetricsNameAppInfoDelimiter) == std::string::npos)
    return false;

  std::vector<std::string> tokens = base::SplitString(
      metrics_name, std::string(1, kMetricsNameAppInfoDelimiter),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK_EQ(tokens.size(), 4u);
  // The order of tokens should match EncodeAppInfoIntoMetricsName().
  *action_name = tokens[0];
  *app_id = tokens[1];
  *session_id = tokens[2];
  *sdk_version = tokens[3];
  return true;
}

// static
std::string CastMetricsHelper::EncodeAppInfoIntoMetricsName(
    const std::string& action_name,
    const std::string& app_id,
    const std::string& session_id,
    const std::string& sdk_version) {
  std::string result(action_name);
  result.push_back(kMetricsNameAppInfoDelimiter);
  result.append(app_id);
  result.push_back(kMetricsNameAppInfoDelimiter);
  result.append(session_id);
  result.push_back(kMetricsNameAppInfoDelimiter);
  result.append(sdk_version);
  return result;
}

// static
CastMetricsHelper* CastMetricsHelper::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

CastMetricsHelper::CastMetricsHelper(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      metrics_sink_(NULL),
      logged_first_audio_(false),
      record_action_callback_(base::Bind(&base::RecordComputedAction)) {
  DCHECK(task_runner_.get());
  DCHECK(!g_instance);
  g_instance = this;
}

CastMetricsHelper::CastMetricsHelper()
    : metrics_sink_(NULL), logged_first_audio_(false) {
  DCHECK(!g_instance);
  g_instance = this;
}

CastMetricsHelper::~CastMetricsHelper() {
  DCHECK_EQ(g_instance, this);
  g_instance = NULL;
}

void CastMetricsHelper::UpdateCurrentAppInfo(const std::string& app_id,
                                             const std::string& session_id) {
  MAKE_SURE_THREAD(UpdateCurrentAppInfo, app_id, session_id);
  app_id_ = app_id;
  session_id_ = session_id;
  app_start_time_ = base::TimeTicks::Now();
  logged_first_audio_ = false;
  TagAppStartForGroupedHistograms(app_id_);
  sdk_version_.clear();
}

void CastMetricsHelper::UpdateSDKInfo(const std::string& sdk_version) {
  MAKE_SURE_THREAD(UpdateSDKInfo, sdk_version);
  sdk_version_ = sdk_version;
}

void CastMetricsHelper::LogMediaPlay() {
  MAKE_SURE_THREAD(LogMediaPlay);
  RecordSimpleAction(EncodeAppInfoIntoMetricsName(
      "MediaPlay",
      app_id_,
      session_id_,
      sdk_version_));
}

void CastMetricsHelper::LogMediaPause() {
  MAKE_SURE_THREAD(LogMediaPause);
  RecordSimpleAction(EncodeAppInfoIntoMetricsName(
      "MediaPause",
      app_id_,
      session_id_,
      sdk_version_));
}

void CastMetricsHelper::LogTimeToFirstPaint() {
  MAKE_SURE_THREAD(LogTimeToFirstPaint);
  if (app_id_.empty())
    return;
  base::TimeDelta launch_time = base::TimeTicks::Now() - app_start_time_;
  const std::string uma_name(GetMetricsNameWithAppName("Startup",
                                                       "TimeToFirstPaint"));
  LogMediumTimeHistogramEvent(uma_name, launch_time);
  LOG(INFO) << uma_name << " is " << launch_time.InSecondsF() << " seconds.";
}

void CastMetricsHelper::LogTimeToFirstAudio() {
  MAKE_SURE_THREAD(LogTimeToFirstAudio);
  if (logged_first_audio_)
    return;
  if (app_id_.empty())
    return;
  base::TimeDelta time_to_first_audio =
      base::TimeTicks::Now() - app_start_time_;
  const std::string uma_name(
      GetMetricsNameWithAppName("Startup", "TimeToFirstAudio"));
  LogMediumTimeHistogramEvent(uma_name, time_to_first_audio);
  LOG(INFO) << uma_name << " is " << time_to_first_audio.InSecondsF()
            << " seconds.";
  logged_first_audio_ = true;
}

void CastMetricsHelper::LogTimeToBufferAv(BufferingType buffering_type,
                                          base::TimeDelta time) {
  MAKE_SURE_THREAD(LogTimeToBufferAv, buffering_type, time);
  if (time < base::TimeDelta::FromSeconds(0)) {
    LOG(WARNING) << "Negative time";
    return;
  }

  const std::string uma_name(GetMetricsNameWithAppName(
      "Media",
      (buffering_type == kInitialBuffering ? "TimeToBufferAv" :
       buffering_type == kBufferingAfterUnderrun ?
           "TimeToBufferAvAfterUnderrun" :
       buffering_type == kAbortedBuffering ? "TimeToBufferAvAfterAbort" : "")));

  // Histogram from 250ms to 30s with 50 buckets.
  // The ratio between 2 consecutive buckets is:
  // exp( (ln(30000) - ln(250)) / 50 ) = 1.1
  LogTimeHistogramEvent(
      uma_name,
      time,
      base::TimeDelta::FromMilliseconds(250),
      base::TimeDelta::FromMilliseconds(30000),
      50);
}

std::string CastMetricsHelper::GetMetricsNameWithAppName(
    const std::string& prefix,
    const std::string& suffix) const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  std::string metrics_name(prefix);
  if (!app_id_.empty()) {
    if (!metrics_name.empty())
      metrics_name.push_back('.');
    metrics_name.append(app_id_);
  }
  if (!suffix.empty()) {
    if (!metrics_name.empty())
      metrics_name.push_back('.');
    metrics_name.append(suffix);
  }
  return metrics_name;
}

void CastMetricsHelper::SetMetricsSink(MetricsSink* delegate) {
  MAKE_SURE_THREAD(SetMetricsSink, delegate);
  metrics_sink_ = delegate;
}

void CastMetricsHelper::SetRecordActionCallback(
      const RecordActionCallback& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  record_action_callback_ = callback;
}

void CastMetricsHelper::SetDummySessionIdForTesting() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  session_id_ = "00000000-0000-0000-0000-000000000000";
}

void CastMetricsHelper::RecordSimpleAction(const std::string& action) {
  MAKE_SURE_THREAD(RecordSimpleAction, action);

  if (metrics_sink_) {
    metrics_sink_->OnAction(action);
  } else {
    record_action_callback_.Run(action);
  }
}

void CastMetricsHelper::LogEnumerationHistogramEvent(
    const std::string& name, int value, int num_buckets) {
  MAKE_SURE_THREAD(LogEnumerationHistogramEvent, name, value, num_buckets);

  if (metrics_sink_) {
    metrics_sink_->OnEnumerationEvent(name, value, num_buckets);
  } else {
    UMA_HISTOGRAM_ENUMERATION_NO_CACHE(name, value, num_buckets);
  }
}

void CastMetricsHelper::LogTimeHistogramEvent(const std::string& name,
                                              const base::TimeDelta& value,
                                              const base::TimeDelta& min,
                                              const base::TimeDelta& max,
                                              int num_buckets) {
  MAKE_SURE_THREAD(LogTimeHistogramEvent, name, value, min, max, num_buckets);

  if (metrics_sink_) {
    metrics_sink_->OnTimeEvent(name, value, min, max, num_buckets);
  } else {
    UMA_HISTOGRAM_CUSTOM_TIMES_NO_CACHE(name, value, min, max, num_buckets);
  }
}

void CastMetricsHelper::LogMediumTimeHistogramEvent(
    const std::string& name,
    const base::TimeDelta& value) {
  // Follow UMA_HISTOGRAM_MEDIUM_TIMES definition.
  LogTimeHistogramEvent(name, value,
                        base::TimeDelta::FromMilliseconds(10),
                        base::TimeDelta::FromMinutes(3),
                        50);
}

void CastMetricsHelper::RecordEventWithValue(const std::string& event,
                                             int value) {
  std::unique_ptr<base::DictionaryValue> cast_event(CreateEventBase(event));
  cast_event->SetInteger("value", value);
  const std::string message = *SerializeToJson(*cast_event);
  RecordSimpleAction(message);
}

void CastMetricsHelper::RecordApplicationEvent(const std::string& event) {
  std::unique_ptr<base::DictionaryValue> cast_event(CreateEventBase(event));
  cast_event->SetString("app_id", app_id_);
  cast_event->SetString("session_id", session_id_);
  cast_event->SetString("sdk_version", sdk_version_);
  const std::string message = *SerializeToJson(*cast_event);
  RecordSimpleAction(message);
}

void CastMetricsHelper::RecordApplicationEventWithValue(
    const std::string& event,
    int value) {
  std::unique_ptr<base::DictionaryValue> cast_event(CreateEventBase(event));
  cast_event->SetString("app_id", app_id_);
  cast_event->SetString("session_id", session_id_);
  cast_event->SetString("sdk_version", sdk_version_);
  cast_event->SetInteger("value", value);
  const std::string message = *SerializeToJson(*cast_event);
  RecordSimpleAction(message);
}

}  // namespace metrics
}  // namespace chromecast
