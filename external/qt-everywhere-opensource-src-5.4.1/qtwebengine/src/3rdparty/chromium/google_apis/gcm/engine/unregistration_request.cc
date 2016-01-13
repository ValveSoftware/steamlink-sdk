// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/unregistration_request.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "net/base/escape.h"
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
const char kUnregistrationCallerKey[] = "gcm_unreg_caller";
// We are going to set the value to "false" in order to forcefully unregister
// the application.
const char kUnregistrationCallerValue[] = "false";

// Response constants.
const char kDeletedPrefix[] = "deleted=";
const char kErrorPrefix[] = "Error=";
const char kInvalidParameters[] = "INVALID_PARAMETERS";


void BuildFormEncoding(const std::string& key,
                       const std::string& value,
                       std::string* out) {
  if (!out->empty())
    out->append("&");
  out->append(key + "=" + net::EscapeUrlEncodedData(value, true));
}

UnregistrationRequest::Status ParseFetcherResponse(
    const net::URLFetcher* source,
    std::string request_app_id) {
  if (!source->GetStatus().is_success()) {
    DVLOG(1) << "Fetcher failed";
    return UnregistrationRequest::URL_FETCHING_FAILED;
  }

  net::HttpStatusCode response_status = static_cast<net::HttpStatusCode>(
      source->GetResponseCode());
  if (response_status != net::HTTP_OK) {
    DVLOG(1) << "HTTP Status code is not OK, but: " << response_status;
    if (response_status == net::HTTP_SERVICE_UNAVAILABLE)
      return UnregistrationRequest::SERVICE_UNAVAILABLE;
    else if (response_status == net::HTTP_INTERNAL_SERVER_ERROR)
      return UnregistrationRequest::INTERNAL_SERVER_ERROR;
    return UnregistrationRequest::HTTP_NOT_OK;
  }

  std::string response;
  if (!source->GetResponseAsString(&response)) {
    DVLOG(1) << "Failed to get response body.";
    return UnregistrationRequest::NO_RESPONSE_BODY;
  }

  DVLOG(1) << "Parsing unregistration response.";
  if (response.find(kDeletedPrefix) != std::string::npos) {
    std::string app_id = response.substr(
        response.find(kDeletedPrefix) + arraysize(kDeletedPrefix) - 1);
    if (app_id == request_app_id)
      return UnregistrationRequest::SUCCESS;
    return UnregistrationRequest::INCORRECT_APP_ID;
  }

  if (response.find(kErrorPrefix) != std::string::npos) {
    std::string error = response.substr(
        response.find(kErrorPrefix) + arraysize(kErrorPrefix) - 1);
    if (error == kInvalidParameters)
      return UnregistrationRequest::INVALID_PARAMETERS;
    return UnregistrationRequest::UNKNOWN_ERROR;
  }

  DVLOG(1) << "Not able to parse a meaningful output from response body."
           << response;
  return UnregistrationRequest::RESPONSE_PARSING_FAILED;
}

}  // namespace

UnregistrationRequest::RequestInfo::RequestInfo(
    uint64 android_id,
    uint64 security_token,
    const std::string& app_id)
    : android_id(android_id),
      security_token(security_token),
      app_id(app_id) {
}

UnregistrationRequest::RequestInfo::~RequestInfo() {}

UnregistrationRequest::UnregistrationRequest(
    const GURL& registration_url,
    const RequestInfo& request_info,
    const net::BackoffEntry::Policy& backoff_policy,
    const UnregistrationCallback& callback,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    GCMStatsRecorder* recorder)
    : callback_(callback),
      request_info_(request_info),
      registration_url_(registration_url),
      backoff_entry_(&backoff_policy),
      request_context_getter_(request_context_getter),
      recorder_(recorder),
      weak_ptr_factory_(this) {
}

UnregistrationRequest::~UnregistrationRequest() {}

void UnregistrationRequest::Start() {
  DCHECK(!callback_.is_null());
  DCHECK(request_info_.android_id != 0UL);
  DCHECK(request_info_.security_token != 0UL);
  DCHECK(!url_fetcher_.get());

  url_fetcher_.reset(net::URLFetcher::Create(
      registration_url_, net::URLFetcher::POST, this));
  url_fetcher_->SetRequestContext(request_context_getter_);

  std::string android_id = base::Uint64ToString(request_info_.android_id);
  std::string auth_header =
      std::string(kLoginHeader) + " " + android_id + ":" +
      base::Uint64ToString(request_info_.security_token);
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kAuthorization, auth_header);
  headers.SetHeader(kAppIdKey, request_info_.app_id);
  url_fetcher_->SetExtraRequestHeaders(headers.ToString());

  std::string body;
  BuildFormEncoding(kAppIdKey, request_info_.app_id, &body);
  BuildFormEncoding(kDeviceIdKey, android_id, &body);
  BuildFormEncoding(kDeleteKey, kDeleteValue, &body);
  BuildFormEncoding(kUnregistrationCallerKey,
                    kUnregistrationCallerValue,
                    &body);

  DVLOG(1) << "Unregistration request: " << body;
  url_fetcher_->SetUploadData(kRequestContentType, body);

  DVLOG(1) << "Performing unregistration for: " << request_info_.app_id;
  recorder_->RecordUnregistrationSent(request_info_.app_id);
  request_start_time_ = base::TimeTicks::Now();
  url_fetcher_->Start();
}

void UnregistrationRequest::RetryWithBackoff(bool update_backoff) {
  if (update_backoff) {
    url_fetcher_.reset();
    backoff_entry_.InformOfRequest(false);
  }

  if (backoff_entry_.ShouldRejectRequest()) {
    DVLOG(1) << "Delaying GCM unregistration of app: "
             << request_info_.app_id << ", for "
             << backoff_entry_.GetTimeUntilRelease().InMilliseconds()
             << " milliseconds.";
    recorder_->RecordUnregistrationRetryDelayed(
        request_info_.app_id,
        backoff_entry_.GetTimeUntilRelease().InMilliseconds());
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&UnregistrationRequest::RetryWithBackoff,
                   weak_ptr_factory_.GetWeakPtr(),
                   false),
        backoff_entry_.GetTimeUntilRelease());
    return;
  }

  Start();
}

void UnregistrationRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  UnregistrationRequest::Status status =
      ParseFetcherResponse(source, request_info_.app_id);

  DVLOG(1) << "UnregistrationRequestStauts: " << status;
  UMA_HISTOGRAM_ENUMERATION("GCM.UnregistrationRequestStatus",
                            status,
                            UNREGISTRATION_STATUS_COUNT);
  recorder_->RecordUnregistrationResponse(request_info_.app_id, status);

  if (status == URL_FETCHING_FAILED ||
      status == SERVICE_UNAVAILABLE ||
      status == INTERNAL_SERVER_ERROR ||
      status == INCORRECT_APP_ID ||
      status == RESPONSE_PARSING_FAILED) {
    RetryWithBackoff(true);
    return;
  }

  // status == SUCCESS || HTTP_NOT_OK || NO_RESPONSE_BODY ||
  // INVALID_PARAMETERS || UNKNOWN_ERROR

  if (status == SUCCESS) {
    UMA_HISTOGRAM_COUNTS("GCM.UnregistrationRetryCount",
                         backoff_entry_.failure_count());
    UMA_HISTOGRAM_TIMES("GCM.UnregistrationCompleteTime",
                        base::TimeTicks::Now() - request_start_time_);
  }

  callback_.Run(status);
}

}  // namespace gcm
