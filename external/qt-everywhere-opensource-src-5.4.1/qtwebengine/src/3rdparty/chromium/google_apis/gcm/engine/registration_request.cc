// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/registration_request.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "net/base/escape.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace gcm {

namespace {

const char kRegistrationRequestContentType[] =
    "application/x-www-form-urlencoded";

// Request constants.
const char kAppIdKey[] = "app";
const char kDeviceIdKey[] = "device";
const char kLoginHeader[] = "AidLogin";
const char kSenderKey[] = "sender";

// Request validation constants.
const size_t kMaxSenders = 100;

// Response constants.
const char kErrorPrefix[] = "Error=";
const char kTokenPrefix[] = "token=";
const char kDeviceRegistrationError[] = "PHONE_REGISTRATION_ERROR";
const char kAuthenticationFailed[] = "AUTHENTICATION_FAILED";
const char kInvalidSender[] = "INVALID_SENDER";
const char kInvalidParameters[] = "INVALID_PARAMETERS";

void BuildFormEncoding(const std::string& key,
                       const std::string& value,
                       std::string* out) {
  if (!out->empty())
    out->append("&");
  out->append(key + "=" + net::EscapeUrlEncodedData(value, true));
}

// Gets correct status from the error message.
RegistrationRequest::Status GetStatusFromError(const std::string& error) {
  // TODO(fgorski): Improve error parsing in case there is nore then just an
  // Error=ERROR_STRING in response.
  if (error.find(kDeviceRegistrationError) != std::string::npos)
    return RegistrationRequest::DEVICE_REGISTRATION_ERROR;
  if (error.find(kAuthenticationFailed) != std::string::npos)
    return RegistrationRequest::AUTHENTICATION_FAILED;
  if (error.find(kInvalidSender) != std::string::npos)
    return RegistrationRequest::INVALID_SENDER;
  if (error.find(kInvalidParameters) != std::string::npos)
    return RegistrationRequest::INVALID_PARAMETERS;
  return RegistrationRequest::UNKNOWN_ERROR;
}

// Indicates whether a retry attempt should be made based on the status of the
// last request.
bool ShouldRetryWithStatus(RegistrationRequest::Status status) {
  return status == RegistrationRequest::UNKNOWN_ERROR ||
         status == RegistrationRequest::AUTHENTICATION_FAILED ||
         status == RegistrationRequest::DEVICE_REGISTRATION_ERROR ||
         status == RegistrationRequest::HTTP_NOT_OK ||
         status == RegistrationRequest::URL_FETCHING_FAILED ||
         status == RegistrationRequest::RESPONSE_PARSING_FAILED;
}

void RecordRegistrationStatusToUMA(RegistrationRequest::Status status) {
  UMA_HISTOGRAM_ENUMERATION("GCM.RegistrationRequestStatus", status,
                            RegistrationRequest::STATUS_COUNT);
}

}  // namespace

RegistrationRequest::RequestInfo::RequestInfo(
    uint64 android_id,
    uint64 security_token,
    const std::string& app_id,
    const std::vector<std::string>& sender_ids)
    : android_id(android_id),
      security_token(security_token),
      app_id(app_id),
      sender_ids(sender_ids) {
}

RegistrationRequest::RequestInfo::~RequestInfo() {}

RegistrationRequest::RegistrationRequest(
    const GURL& registration_url,
    const RequestInfo& request_info,
    const net::BackoffEntry::Policy& backoff_policy,
    const RegistrationCallback& callback,
    int max_retry_count,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    GCMStatsRecorder* recorder)
    : callback_(callback),
      request_info_(request_info),
      registration_url_(registration_url),
      backoff_entry_(&backoff_policy),
      request_context_getter_(request_context_getter),
      retries_left_(max_retry_count),
      recorder_(recorder),
      weak_ptr_factory_(this) {
  DCHECK_GE(max_retry_count, 0);
}

RegistrationRequest::~RegistrationRequest() {}

void RegistrationRequest::Start() {
  DCHECK(!callback_.is_null());
  DCHECK(request_info_.android_id != 0UL);
  DCHECK(request_info_.security_token != 0UL);
  DCHECK(0 < request_info_.sender_ids.size() &&
         request_info_.sender_ids.size() <= kMaxSenders);

  DCHECK(!url_fetcher_.get());
  url_fetcher_.reset(net::URLFetcher::Create(
      registration_url_, net::URLFetcher::POST, this));
  url_fetcher_->SetRequestContext(request_context_getter_);

  std::string android_id = base::Uint64ToString(request_info_.android_id);
  std::string auth_header =
      std::string(net::HttpRequestHeaders::kAuthorization) + ": " +
      kLoginHeader + " " + android_id + ":" +
      base::Uint64ToString(request_info_.security_token);
  url_fetcher_->SetExtraRequestHeaders(auth_header);

  std::string body;
  BuildFormEncoding(kAppIdKey, request_info_.app_id, &body);
  BuildFormEncoding(kDeviceIdKey, android_id, &body);

  std::string senders;
  for (std::vector<std::string>::const_iterator iter =
           request_info_.sender_ids.begin();
       iter != request_info_.sender_ids.end();
       ++iter) {
    DCHECK(!iter->empty());
    if (!senders.empty())
      senders.append(",");
    senders.append(*iter);
  }
  BuildFormEncoding(kSenderKey, senders, &body);
  UMA_HISTOGRAM_COUNTS("GCM.RegistrationSenderIdCount",
                       request_info_.sender_ids.size());

  DVLOG(1) << "Performing registration for: " << request_info_.app_id;
  DVLOG(1) << "Registration request: " << body;
  url_fetcher_->SetUploadData(kRegistrationRequestContentType, body);
  recorder_->RecordRegistrationSent(request_info_.app_id, senders);
  request_start_time_ = base::TimeTicks::Now();
  url_fetcher_->Start();
}

void RegistrationRequest::RetryWithBackoff(bool update_backoff) {
  if (update_backoff) {
    DCHECK_GT(retries_left_, 0);
    --retries_left_;
    url_fetcher_.reset();
    backoff_entry_.InformOfRequest(false);
  }

  if (backoff_entry_.ShouldRejectRequest()) {
    DVLOG(1) << "Delaying GCM registration of app: "
             << request_info_.app_id << ", for "
             << backoff_entry_.GetTimeUntilRelease().InMilliseconds()
             << " milliseconds.";
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&RegistrationRequest::RetryWithBackoff,
                   weak_ptr_factory_.GetWeakPtr(),
                   false),
        backoff_entry_.GetTimeUntilRelease());
    return;
  }

  Start();
}

RegistrationRequest::Status RegistrationRequest::ParseResponse(
    const net::URLFetcher* source, std::string* token) {
  if (!source->GetStatus().is_success()) {
    LOG(ERROR) << "URL fetching failed.";
    return URL_FETCHING_FAILED;
  }

  std::string response;
  if (!source->GetResponseAsString(&response)) {
    LOG(ERROR) << "Failed to parse registration response as a string.";
    return RESPONSE_PARSING_FAILED;
  }

  if (source->GetResponseCode() == net::HTTP_OK) {
    size_t token_pos = response.find(kTokenPrefix);
    if (token_pos != std::string::npos) {
      *token = response.substr(token_pos + arraysize(kTokenPrefix) - 1);
      return SUCCESS;
    }
  }

  // If we are able to parse a meaningful known error, let's do so. Some errors
  // will have HTTP_BAD_REQUEST, some will have HTTP_OK response code.
  size_t error_pos = response.find(kErrorPrefix);
  if (error_pos != std::string::npos) {
    std::string error = response.substr(
        error_pos + arraysize(kErrorPrefix) - 1);
    return GetStatusFromError(error);
  }

  // If we cannot tell what the error is, but at least we know response code was
  // not OK.
  if (source->GetResponseCode() != net::HTTP_OK) {
    DLOG(ERROR) << "URL fetching HTTP response code is not OK. It is "
                << source->GetResponseCode();
    return HTTP_NOT_OK;
  }

  return UNKNOWN_ERROR;
}

void RegistrationRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  std::string token;
  Status status = ParseResponse(source, &token);
  RecordRegistrationStatusToUMA(status);
  recorder_->RecordRegistrationResponse(
      request_info_.app_id,
      request_info_.sender_ids,
      status);

  if (ShouldRetryWithStatus(status)) {
    if (retries_left_ > 0) {
      recorder_->RecordRegistrationRetryRequested(
          request_info_.app_id,
          request_info_.sender_ids,
          retries_left_);
      RetryWithBackoff(true);
      return;
    }

    status = REACHED_MAX_RETRIES;
    recorder_->RecordRegistrationResponse(
        request_info_.app_id,
        request_info_.sender_ids,
        status);
    RecordRegistrationStatusToUMA(status);
  }

  if (status == SUCCESS) {
    UMA_HISTOGRAM_COUNTS("GCM.RegistrationRetryCount",
                         backoff_entry_.failure_count());
    UMA_HISTOGRAM_TIMES("GCM.RegistrationCompleteTime",
                        base::TimeTicks::Now() - request_start_time_);
  }
  callback_.Run(status, token);
}

}  // namespace gcm
