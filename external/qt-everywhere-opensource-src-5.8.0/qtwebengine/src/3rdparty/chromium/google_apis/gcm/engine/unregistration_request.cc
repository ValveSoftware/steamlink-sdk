// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/unregistration_request.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "google_apis/gcm/base/gcm_util.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace gcm {

namespace {

const char kRequestContentType[] = "application/x-www-form-urlencoded";

// Request constants.
const char kAppIdKey[] = "app";
const char kDeleteKey[] = "delete";
const char kDeleteValue[] = "true";
const char kDeviceIdKey[] = "device";
const char kLoginHeader[] = "AidLogin";

}  // namespace

UnregistrationRequest::RequestInfo::RequestInfo(uint64_t android_id,
                                                uint64_t security_token,
                                                const std::string& app_id)
    : android_id(android_id), security_token(security_token), app_id(app_id) {
  DCHECK(android_id != 0UL);
  DCHECK(security_token != 0UL);
}

UnregistrationRequest::RequestInfo::~RequestInfo() {}

UnregistrationRequest::CustomRequestHandler::CustomRequestHandler() {}

UnregistrationRequest::CustomRequestHandler::~CustomRequestHandler() {}

UnregistrationRequest::UnregistrationRequest(
    const GURL& registration_url,
    const RequestInfo& request_info,
    std::unique_ptr<CustomRequestHandler> custom_request_handler,
    const net::BackoffEntry::Policy& backoff_policy,
    const UnregistrationCallback& callback,
    int max_retry_count,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    GCMStatsRecorder* recorder,
    const std::string& source_to_record)
    : callback_(callback),
      request_info_(request_info),
      custom_request_handler_(std::move(custom_request_handler)),
      registration_url_(registration_url),
      backoff_entry_(&backoff_policy),
      request_context_getter_(request_context_getter),
      retries_left_(max_retry_count),
      recorder_(recorder),
      source_to_record_(source_to_record),
      weak_ptr_factory_(this) {
  DCHECK_GE(max_retry_count, 0);
}

UnregistrationRequest::~UnregistrationRequest() {}

void UnregistrationRequest::Start() {
  DCHECK(!callback_.is_null());
  DCHECK(!url_fetcher_.get());

  url_fetcher_ =
      net::URLFetcher::Create(registration_url_, net::URLFetcher::POST, this);
  url_fetcher_->SetRequestContext(request_context_getter_.get());
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES);

  std::string extra_headers;
  BuildRequestHeaders(&extra_headers);
  url_fetcher_->SetExtraRequestHeaders(extra_headers);

  std::string body;
  BuildRequestBody(&body);

  DVLOG(1) << "Unregistration request: " << body;
  url_fetcher_->SetUploadData(kRequestContentType, body);

  DVLOG(1) << "Performing unregistration for: " << request_info_.app_id;
  recorder_->RecordUnregistrationSent(request_info_.app_id, source_to_record_);
  request_start_time_ = base::TimeTicks::Now();
  url_fetcher_->Start();
}

void UnregistrationRequest::BuildRequestHeaders(std::string* extra_headers) {
  net::HttpRequestHeaders headers;
  headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      std::string(kLoginHeader) + " " +
          base::Uint64ToString(request_info_.android_id) + ":" +
          base::Uint64ToString(request_info_.security_token));
  headers.SetHeader(kAppIdKey, request_info_.app_id);
  *extra_headers = headers.ToString();
}

void UnregistrationRequest::BuildRequestBody(std::string* body) {
  BuildFormEncoding(kAppIdKey, request_info_.app_id, body);
  BuildFormEncoding(kDeviceIdKey,
                    base::Uint64ToString(request_info_.android_id),
                    body);
  BuildFormEncoding(kDeleteKey, kDeleteValue, body);

  DCHECK(custom_request_handler_.get());
  custom_request_handler_->BuildRequestBody(body);
}

UnregistrationRequest::Status UnregistrationRequest::ParseResponse(
    const net::URLFetcher* source) {
  if (!source->GetStatus().is_success()) {
    DVLOG(1) << "Fetcher failed";
    return URL_FETCHING_FAILED;
  }

  net::HttpStatusCode response_status = static_cast<net::HttpStatusCode>(
      source->GetResponseCode());
  if (response_status != net::HTTP_OK) {
    DVLOG(1) << "HTTP Status code is not OK, but: " << response_status;
    if (response_status == net::HTTP_SERVICE_UNAVAILABLE)
      return SERVICE_UNAVAILABLE;
    if (response_status == net::HTTP_INTERNAL_SERVER_ERROR)
      return INTERNAL_SERVER_ERROR;
    return HTTP_NOT_OK;
  }

  DCHECK(custom_request_handler_.get());
  return custom_request_handler_->ParseResponse(source);
}

void UnregistrationRequest::RetryWithBackoff() {
  DCHECK_GT(retries_left_, 0);
  --retries_left_;
  url_fetcher_.reset();
  backoff_entry_.InformOfRequest(false);

  DVLOG(1) << "Delaying GCM unregistration of app: "
           << request_info_.app_id << ", for "
           << backoff_entry_.GetTimeUntilRelease().InMilliseconds()
           << " milliseconds.";
  recorder_->RecordUnregistrationRetryDelayed(
      request_info_.app_id,
      source_to_record_,
      backoff_entry_.GetTimeUntilRelease().InMilliseconds(),
      retries_left_ + 1);
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&UnregistrationRequest::Start, weak_ptr_factory_.GetWeakPtr()),
      backoff_entry_.GetTimeUntilRelease());
}

void UnregistrationRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  UnregistrationRequest::Status status = ParseResponse(source);

  DVLOG(1) << "UnregistrationRequestStauts: " << status;

  DCHECK(custom_request_handler_.get());
  custom_request_handler_->ReportUMAs(
      status,
      backoff_entry_.failure_count(),
      base::TimeTicks::Now() - request_start_time_);

  recorder_->RecordUnregistrationResponse(
      request_info_.app_id, source_to_record_, status);

  if (status == URL_FETCHING_FAILED ||
      status == HTTP_NOT_OK ||
      status == NO_RESPONSE_BODY ||
      status == SERVICE_UNAVAILABLE ||
      status == INTERNAL_SERVER_ERROR ||
      status == INCORRECT_APP_ID ||
      status == RESPONSE_PARSING_FAILED) {
    if (retries_left_ > 0) {
      RetryWithBackoff();
      return;
    }

    status = REACHED_MAX_RETRIES;
    recorder_->RecordUnregistrationResponse(
        request_info_.app_id, source_to_record_, status);

    // Only REACHED_MAX_RETRIES is reported because the function will skip
    // reporting count and time when status is not SUCCESS.
    DCHECK(custom_request_handler_.get());
    custom_request_handler_->ReportUMAs(status, 0, base::TimeDelta());
  }

  // status == SUCCESS || INVALID_PARAMETERS || UNKNOWN_ERROR ||
  //           REACHED_MAX_RETRIES

  callback_.Run(status);
}

}  // namespace gcm
