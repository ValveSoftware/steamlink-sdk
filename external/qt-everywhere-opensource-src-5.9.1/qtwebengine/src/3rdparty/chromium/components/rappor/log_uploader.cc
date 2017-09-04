// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/log_uploader.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher.h"

namespace {

// The delay, in seconds, between uploading when there are queued logs to send.
const int kUnsentLogsIntervalSeconds = 3;

// When uploading metrics to the server fails, we progressively wait longer and
// longer before sending the next log. This backoff process helps reduce load
// on a server that is having issues.
// The following is the multiplier we use to expand that inter-log duration.
const double kBackoffMultiplier = 1.1;

// The maximum backoff multiplier.
const int kMaxBackoffIntervalSeconds = 60 * 60;

// The maximum number of unsent logs we will keep.
// TODO(holte): Limit based on log size instead.
const size_t kMaxQueuedLogs = 10;

enum DiscardReason {
  UPLOAD_SUCCESS,
  UPLOAD_REJECTED,
  QUEUE_OVERFLOW,
  NUM_DISCARD_REASONS
};

void RecordDiscardReason(DiscardReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Rappor.DiscardReason",
                            reason,
                            NUM_DISCARD_REASONS);
}

}  // namespace

namespace rappor {

LogUploader::LogUploader(const GURL& server_url,
                         const std::string& mime_type,
                         net::URLRequestContextGetter* request_context)
    : server_url_(server_url),
      mime_type_(mime_type),
      request_context_(request_context),
      is_running_(false),
      has_callback_pending_(false),
      upload_interval_(base::TimeDelta::FromSeconds(
          kUnsentLogsIntervalSeconds)) {
}

LogUploader::~LogUploader() {}

void LogUploader::Start() {
  is_running_ = true;
  StartScheduledUpload();
}

void LogUploader::Stop() {
  is_running_ = false;
  // Rather than interrupting the current upload, just let it finish/fail and
  // then inhibit any retry attempts.
}

void LogUploader::QueueLog(const std::string& log) {
  queued_logs_.push(log);
  // Don't drop logs yet if an upload is in progress.  They will be dropped
  // when it finishes.
  if (!has_callback_pending_)
    DropExcessLogs();
  StartScheduledUpload();
}

void LogUploader::DropExcessLogs() {
  while (queued_logs_.size() > kMaxQueuedLogs) {
    DVLOG(2) << "Dropping excess log.";
    RecordDiscardReason(QUEUE_OVERFLOW);
    queued_logs_.pop();
  }
}

bool LogUploader::IsUploadScheduled() const {
  return upload_timer_.IsRunning();
}

void LogUploader::ScheduleNextUpload(base::TimeDelta interval) {
  upload_timer_.Start(
      FROM_HERE, interval, this, &LogUploader::StartScheduledUpload);
}

bool LogUploader::CanStartUpload() const {
  return is_running_ &&
         !queued_logs_.empty() &&
         !IsUploadScheduled() &&
         !has_callback_pending_;
}

void LogUploader::StartScheduledUpload() {
  if (!CanStartUpload())
    return;
  DVLOG(2) << "Upload to " << server_url_.spec() << " starting.";
  has_callback_pending_ = true;
  current_fetch_ =
      net::URLFetcher::Create(server_url_, net::URLFetcher::POST, this);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      current_fetch_.get(), data_use_measurement::DataUseUserData::RAPPOR);
  current_fetch_->SetRequestContext(request_context_.get());
  current_fetch_->SetUploadData(mime_type_, queued_logs_.front());

  // We already drop cookies server-side, but we might as well strip them out
  // client-side as well.
  current_fetch_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                               net::LOAD_DO_NOT_SEND_COOKIES);
  current_fetch_->Start();
}

// static
base::TimeDelta LogUploader::BackOffUploadInterval(base::TimeDelta interval) {
  DCHECK_GT(kBackoffMultiplier, 1.0);
  interval = base::TimeDelta::FromMicroseconds(
      static_cast<int64_t>(kBackoffMultiplier * interval.InMicroseconds()));

  base::TimeDelta max_interval =
      base::TimeDelta::FromSeconds(kMaxBackoffIntervalSeconds);
  return interval > max_interval ? max_interval : interval;
}

void LogUploader::OnURLFetchComplete(const net::URLFetcher* source) {
  // We're not allowed to re-use the existing |URLFetcher|s, so free them here.
  // Note however that |source| is aliased to the fetcher, so we should be
  // careful not to delete it too early.
  DCHECK_EQ(current_fetch_.get(), source);
  std::unique_ptr<net::URLFetcher> fetch(std::move(current_fetch_));

  const net::URLRequestStatus& request_status = source->GetStatus();

  const int response_code = source->GetResponseCode();
  DVLOG(2) << "Upload fetch complete response code: " << response_code;

  if (request_status.status() != net::URLRequestStatus::SUCCESS) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Rappor.FailedUploadErrorCode",
                                -request_status.error());
    DVLOG(1) << "Rappor server upload failed with error: "
             << request_status.error() << ": "
             << net::ErrorToString(request_status.error());
    DCHECK_EQ(-1, response_code);
  } else {
    // Log a histogram to track response success vs. failure rates.
    UMA_HISTOGRAM_SPARSE_SLOWLY("Rappor.UploadResponseCode", response_code);
  }

  const bool upload_succeeded = response_code == 200;

  // Determine whether this log should be retransmitted.
  DiscardReason reason = NUM_DISCARD_REASONS;
  if (upload_succeeded) {
    reason = UPLOAD_SUCCESS;
  } else if (response_code == 400) {
    reason = UPLOAD_REJECTED;
  }

  if (reason != NUM_DISCARD_REASONS) {
    DVLOG(2) << "Log discarded.";
    RecordDiscardReason(reason);
    queued_logs_.pop();
  }

  DropExcessLogs();

  // Error 400 indicates a problem with the log, not with the server, so
  // don't consider that a sign that the server is in trouble.
  const bool server_is_healthy = upload_succeeded || response_code == 400;
  OnUploadFinished(server_is_healthy);
}

void LogUploader::OnUploadFinished(bool server_is_healthy) {
  DCHECK(has_callback_pending_);
  has_callback_pending_ = false;
  // If the server is having issues, back off. Otherwise, reset to default.
  if (!server_is_healthy)
    upload_interval_ = BackOffUploadInterval(upload_interval_);
  else
    upload_interval_ = base::TimeDelta::FromSeconds(kUnsentLogsIntervalSeconds);

  if (CanStartUpload())
    ScheduleNextUpload(upload_interval_);
}

}  // namespace rappor
