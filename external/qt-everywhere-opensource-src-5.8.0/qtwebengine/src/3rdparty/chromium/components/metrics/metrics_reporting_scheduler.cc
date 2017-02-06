// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_reporting_scheduler.h"

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/variations/variations_associated_data.h"

using base::TimeDelta;

namespace metrics {

namespace {

// The delay, in seconds, after startup before sending the first log message.
#if defined(OS_ANDROID) || defined(OS_IOS)
// Sessions are more likely to be short on a mobile device, so handle the
// initial log quickly.
const int kInitialUploadIntervalSeconds = 15;
#else
const int kInitialUploadIntervalSeconds = 60;
#endif

// The delay, in seconds, between uploading when there are queued logs from
// previous sessions to send.
#if defined(OS_ANDROID) || defined(OS_IOS)
// Sending in a burst is better on a mobile device, since keeping the radio on
// is very expensive.
const int kUnsentLogsIntervalSeconds = 3;
#else
const int kUnsentLogsIntervalSeconds = 15;
#endif

// When uploading metrics to the server fails, we progressively wait longer and
// longer before sending the next log. This backoff process helps reduce load
// on a server that is having issues.
// The following is the multiplier we use to expand that inter-log duration.
const double kBackoffMultiplier = 1.1;

// The maximum backoff multiplier.
const int kMaxBackoffMultiplier = 10;

enum InitSequence {
  TIMER_FIRED_FIRST,
  INIT_TASK_COMPLETED_FIRST,
  INIT_SEQUENCE_ENUM_SIZE,
};

void LogMetricsInitSequence(InitSequence sequence) {
  UMA_HISTOGRAM_ENUMERATION("UMA.InitSequence", sequence,
                            INIT_SEQUENCE_ENUM_SIZE);
}

void LogActualUploadInterval(TimeDelta interval) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("UMA.ActualLogUploadInterval",
                             interval.InMinutes(),
                             1,
                             base::TimeDelta::FromHours(12).InMinutes(),
                             50);
}

}  // anonymous namespace

MetricsReportingScheduler::MetricsReportingScheduler(
    const base::Closure& upload_callback,
    const base::Callback<base::TimeDelta(void)>& upload_interval_callback)
    : upload_callback_(upload_callback),
      upload_interval_(TimeDelta::FromSeconds(kInitialUploadIntervalSeconds)),
      running_(false),
      callback_pending_(false),
      init_task_complete_(false),
      waiting_for_init_task_complete_(false),
      upload_interval_callback_(upload_interval_callback) {
}

MetricsReportingScheduler::~MetricsReportingScheduler() {}

void MetricsReportingScheduler::Start() {
  running_ = true;
  ScheduleNextUpload();
}

void MetricsReportingScheduler::Stop() {
  running_ = false;
  if (upload_timer_.IsRunning())
    upload_timer_.Stop();
}

// Callback from MetricsService when the startup init task has completed.
void MetricsReportingScheduler::InitTaskComplete() {
  DCHECK(!init_task_complete_);
  init_task_complete_ = true;
  if (waiting_for_init_task_complete_) {
    waiting_for_init_task_complete_ = false;
    TriggerUpload();
  } else {
    LogMetricsInitSequence(INIT_TASK_COMPLETED_FIRST);
  }
}

void MetricsReportingScheduler::UploadFinished(bool server_is_healthy,
                                               bool more_logs_remaining) {
  DCHECK(callback_pending_);
  callback_pending_ = false;
  // If the server is having issues, back off. Otherwise, reset to default
  // (unless there are more logs to send, in which case the next upload should
  // happen sooner).
  if (!server_is_healthy) {
    BackOffUploadInterval();
  } else if (more_logs_remaining) {
    upload_interval_ = TimeDelta::FromSeconds(kUnsentLogsIntervalSeconds);
  } else {
    upload_interval_ = GetStandardUploadInterval();
    last_upload_finish_time_ = base::TimeTicks::Now();
  }

  if (running_)
    ScheduleNextUpload();
}

void MetricsReportingScheduler::UploadCancelled() {
  DCHECK(callback_pending_);
  callback_pending_ = false;
  if (running_)
    ScheduleNextUpload();
}

void MetricsReportingScheduler::SetUploadIntervalForTesting(
    base::TimeDelta interval) {
  upload_interval_ = interval;
}

void MetricsReportingScheduler::TriggerUpload() {
  // If the timer fired before the init task has completed, don't trigger the
  // upload yet - wait for the init task to complete and do it then.
  if (!init_task_complete_) {
    LogMetricsInitSequence(TIMER_FIRED_FIRST);
    waiting_for_init_task_complete_ = true;
    return;
  }

  if (!last_upload_finish_time_.is_null()) {
    LogActualUploadInterval(base::TimeTicks::Now() - last_upload_finish_time_);
    last_upload_finish_time_ = base::TimeTicks();
  }

  callback_pending_ = true;
  upload_callback_.Run();
}

void MetricsReportingScheduler::ScheduleNextUpload() {
  DCHECK(running_);
  if (upload_timer_.IsRunning() || callback_pending_)
    return;

  upload_timer_.Start(FROM_HERE, upload_interval_, this,
                      &MetricsReportingScheduler::TriggerUpload);
}

void MetricsReportingScheduler::BackOffUploadInterval() {
  DCHECK_GT(kBackoffMultiplier, 1.0);
  upload_interval_ = TimeDelta::FromMicroseconds(static_cast<int64_t>(
      kBackoffMultiplier * upload_interval_.InMicroseconds()));

  TimeDelta max_interval = kMaxBackoffMultiplier * GetStandardUploadInterval();
  if (upload_interval_ > max_interval || upload_interval_.InSeconds() < 0) {
    upload_interval_ = max_interval;
  }
}

base::TimeDelta MetricsReportingScheduler::GetStandardUploadInterval() {
  return upload_interval_callback_.Run();
}

}  // namespace metrics
