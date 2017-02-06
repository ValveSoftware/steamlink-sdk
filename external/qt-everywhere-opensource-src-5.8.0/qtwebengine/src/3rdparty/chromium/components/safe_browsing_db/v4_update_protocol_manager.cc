// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_update_protocol_manager.h"

#include <utility>

#include "base/base64url.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/timer/timer.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

using base::Time;
using base::TimeDelta;

namespace {

// Enumerate parsing failures for histogramming purposes.  DO NOT CHANGE
// THE ORDERING OF THESE VALUES.
enum ParseResultType {
  // Error parsing the protocol buffer from a string.
  PARSE_FROM_STRING_ERROR = 0,

  // No platform_type set in the response.
  NO_PLATFORM_TYPE_ERROR = 1,

  // No threat_entry_type set in the response.
  NO_THREAT_ENTRY_TYPE_ERROR = 2,

  // No threat_type set in the response.
  NO_THREAT_TYPE_ERROR = 3,

  // No state set in the response for one or more lists.
  NO_STATE_ERROR = 4,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  PARSE_RESULT_TYPE_MAX = 5
};

// Record parsing errors of an update result.
void RecordParseUpdateResult(ParseResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.ParseV4UpdateResult", result_type,
                            PARSE_RESULT_TYPE_MAX);
}

void RecordUpdateResult(safe_browsing::V4OperationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.V4UpdateResult", result,
      safe_browsing::V4OperationResult::OPERATION_RESULT_MAX);
}

}  // namespace

namespace safe_browsing {

// Minimum time, in seconds, from start up before we must issue an update query.
static const int kV4TimerStartIntervalSecMin = 60;

// Maximum time, in seconds, from start up before we must issue an update query.
static const int kV4TimerStartIntervalSecMax = 300;

// The default V4UpdateProtocolManagerFactory.
class V4UpdateProtocolManagerFactoryImpl
    : public V4UpdateProtocolManagerFactory {
 public:
  V4UpdateProtocolManagerFactoryImpl() {}
  ~V4UpdateProtocolManagerFactoryImpl() override {}
  std::unique_ptr<V4UpdateProtocolManager> CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config,
      V4UpdateCallback callback) override {
    return std::unique_ptr<V4UpdateProtocolManager>(
        new V4UpdateProtocolManager(request_context_getter, config, callback));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(V4UpdateProtocolManagerFactoryImpl);
};

// V4UpdateProtocolManager implementation --------------------------------

// static
V4UpdateProtocolManagerFactory* V4UpdateProtocolManager::factory_ = NULL;

// static
std::unique_ptr<V4UpdateProtocolManager> V4UpdateProtocolManager::Create(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config,
    V4UpdateCallback callback) {
  if (!factory_) {
    factory_ = new V4UpdateProtocolManagerFactoryImpl();
  }
  return factory_->CreateProtocolManager(request_context_getter, config,
                                         callback);
}

void V4UpdateProtocolManager::ResetUpdateErrors() {
  update_error_count_ = 0;
  update_back_off_mult_ = 1;
}

V4UpdateProtocolManager::V4UpdateProtocolManager(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config,
    V4UpdateCallback update_callback)
    : update_error_count_(0),
      update_back_off_mult_(1),
      next_update_interval_(base::TimeDelta::FromSeconds(
          base::RandInt(kV4TimerStartIntervalSecMin,
                        kV4TimerStartIntervalSecMax))),
      config_(config),
      request_context_getter_(request_context_getter),
      url_fetcher_id_(0),
      update_callback_(update_callback) {
  // Do not auto-schedule updates. Let the owner (V4LocalDatabaseManager) do it
  // when it is ready to process updates.
}

V4UpdateProtocolManager::~V4UpdateProtocolManager() {}

bool V4UpdateProtocolManager::IsUpdateScheduled() const {
  return update_timer_.IsRunning();
}

void V4UpdateProtocolManager::ScheduleNextUpdate(
    std::unique_ptr<StoreStateMap> store_state_map) {
  store_state_map_ = std::move(store_state_map);
  ScheduleNextUpdateWithBackoff(false);
}

void V4UpdateProtocolManager::ScheduleNextUpdateWithBackoff(bool back_off) {
  DCHECK(CalledOnValidThread());

  // TODO(vakh): Set disable_auto_update correctly using the command line
  // switch.
  if (config_.disable_auto_update) {
    DCHECK(!IsUpdateScheduled());
    return;
  }

  // Reschedule with the new update.
  base::TimeDelta next_update_interval = GetNextUpdateInterval(back_off);
  ScheduleNextUpdateAfterInterval(next_update_interval);
}

// According to section 5 of the SafeBrowsing protocol specification, we must
// back off after a certain number of errors.
base::TimeDelta V4UpdateProtocolManager::GetNextUpdateInterval(bool back_off) {
  DCHECK(CalledOnValidThread());
  DCHECK(next_update_interval_ > base::TimeDelta());

  base::TimeDelta next = next_update_interval_;
  if (back_off) {
    next = V4ProtocolManagerUtil::GetNextBackOffInterval(
        &update_error_count_, &update_back_off_mult_);
  }

  if (!last_response_time_.is_null()) {
    // The callback spent some time updating the database, including disk I/O.
    // Do not wait that extra time.
    base::TimeDelta callback_time = Time::Now() - last_response_time_;
    if (callback_time < next) {
      next -= callback_time;
    } else {
      // If the callback took too long, schedule the next update with no delay.
      next = base::TimeDelta();
    }
  }
  DVLOG(1) << "V4UpdateProtocolManager::GetNextUpdateInterval: "
           << "next_interval: " << next;
  return next;
}

void V4UpdateProtocolManager::ScheduleNextUpdateAfterInterval(
    base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  DCHECK(interval >= base::TimeDelta());

  // Unschedule any current timer.
  update_timer_.Stop();
  update_timer_.Start(FROM_HERE, interval, this,
                      &V4UpdateProtocolManager::IssueUpdateRequest);
}

// static
std::string V4UpdateProtocolManager::GetBase64SerializedUpdateRequestProto(
    const StoreStateMap& store_state_map) {
  DCHECK(!store_state_map.empty());
  // Build the request. Client info and client states are not added to the
  // request protocol buffer. Client info is passed as params in the url.
  FetchThreatListUpdatesRequest request;
  for (const auto& entry : store_state_map) {
    const auto& list_to_update = entry.first;
    const auto& state = entry.second;
    ListUpdateRequest* list_update_request = request.add_list_update_requests();
    list_update_request->set_platform_type(list_to_update.platform_type);
    list_update_request->set_threat_entry_type(
        list_to_update.threat_entry_type);
    list_update_request->set_threat_type(list_to_update.threat_type);

    if (!state.empty()) {
      list_update_request->set_state(state);
    }

    // TODO(vakh): Accept other compression formats also.
    // See: https://bugs.chromium.org/p/chromium/issues/detail?id=624567
    list_update_request->mutable_constraints()->add_supported_compressions(RAW);
  }

  // Serialize and Base64 encode.
  std::string req_data, req_base64;
  request.SerializeToString(&req_data);
  base::Base64UrlEncode(req_data, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &req_base64);
  return req_base64;
}

bool V4UpdateProtocolManager::ParseUpdateResponse(
    const std::string& data,
    ParsedServerResponse* parsed_server_response) {
  FetchThreatListUpdatesResponse response;

  if (!response.ParseFromString(data)) {
    RecordParseUpdateResult(PARSE_FROM_STRING_ERROR);
    return false;
  }

  if (response.has_minimum_wait_duration()) {
    // Seconds resolution is good enough so we ignore the nanos field.
    int64_t minimum_wait_duration_seconds =
        response.minimum_wait_duration().seconds();

    // Do not let the next_update_interval_ to be too low.
    if (minimum_wait_duration_seconds < kV4TimerStartIntervalSecMin) {
      minimum_wait_duration_seconds = kV4TimerStartIntervalSecMin;
    }
    next_update_interval_ =
        base::TimeDelta::FromSeconds(minimum_wait_duration_seconds);
  }

  // TODO(vakh): Do something useful with this response.
  for (ListUpdateResponse& list_update_response :
       *response.mutable_list_update_responses()) {
    if (!list_update_response.has_platform_type()) {
      RecordParseUpdateResult(NO_PLATFORM_TYPE_ERROR);
    } else if (!list_update_response.has_threat_entry_type()) {
      RecordParseUpdateResult(NO_THREAT_ENTRY_TYPE_ERROR);
    } else if (!list_update_response.has_threat_type()) {
      RecordParseUpdateResult(NO_THREAT_TYPE_ERROR);
    } else if (!list_update_response.has_new_client_state()) {
      RecordParseUpdateResult(NO_STATE_ERROR);
    } else {
      std::unique_ptr<ListUpdateResponse> add(new ListUpdateResponse);
      add->Swap(&list_update_response);
      parsed_server_response->push_back(std::move(add));
    }
  }
  return true;
}

void V4UpdateProtocolManager::IssueUpdateRequest() {
  DCHECK(CalledOnValidThread());

  // If an update request is already pending, record and return silently.
  if (request_.get()) {
    RecordUpdateResult(V4OperationResult::ALREADY_PENDING_ERROR);
    return;
  }

  std::string req_base64 =
      GetBase64SerializedUpdateRequestProto(*store_state_map_.get());
  GURL update_url;
  net::HttpRequestHeaders headers;
  GetUpdateUrlAndHeaders(req_base64, &update_url, &headers);

  std::unique_ptr<net::URLFetcher> fetcher = net::URLFetcher::Create(
      url_fetcher_id_++, update_url, net::URLFetcher::GET, this);
  fetcher->SetExtraRequestHeaders(headers.ToString());

  request_.reset(fetcher.release());

  request_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  request_->SetRequestContext(request_context_getter_.get());
  request_->Start();
  // TODO(vakh): Handle request timeout.
}

// net::URLFetcherDelegate implementation ----------------------------------

// SafeBrowsing request responses are handled here.
void V4UpdateProtocolManager::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());

  int response_code = source->GetResponseCode();
  net::URLRequestStatus status = source->GetStatus();
  V4ProtocolManagerUtil::RecordHttpResponseOrErrorCode(
      "SafeBrowsing.V4UpdateHttpResponseOrErrorCode", status, response_code);

  last_response_time_ = Time::Now();

  std::unique_ptr<ParsedServerResponse> parsed_server_response(
      new ParsedServerResponse);
  if (status.is_success() && response_code == net::HTTP_OK) {
    RecordUpdateResult(V4OperationResult::STATUS_200);
    ResetUpdateErrors();
    std::string data;
    source->GetResponseAsString(&data);
    if (!ParseUpdateResponse(data, parsed_server_response.get())) {
      parsed_server_response->clear();
      RecordUpdateResult(V4OperationResult::PARSE_ERROR);
    }
    request_.reset();

    UMA_HISTOGRAM_COUNTS("SafeBrowsing.V4UpdateResponseSizeKB",
                         data.size() / 1024);

    // The caller should update its state now, based on parsed_server_response.
    // The callback must call ScheduleNextUpdate() at the end to resume
    // downloading updates.
    update_callback_.Run(std::move(parsed_server_response));
  } else {
    DVLOG(1) << "SafeBrowsing GetEncodedUpdates request for: "
             << source->GetURL() << " failed with error: " << status.error()
             << " and response code: " << response_code;

    if (status.status() == net::URLRequestStatus::FAILED) {
      RecordUpdateResult(V4OperationResult::NETWORK_ERROR);
    } else {
      RecordUpdateResult(V4OperationResult::HTTP_ERROR);
    }
    // TODO(vakh): Figure out whether it is just a network error vs backoff vs
    // another condition and RecordUpdateResult more accurately.

    request_.reset();
    ScheduleNextUpdateWithBackoff(true);
  }
}

void V4UpdateProtocolManager::GetUpdateUrlAndHeaders(
    const std::string& req_base64,
    GURL* gurl,
    net::HttpRequestHeaders* headers) const {
  V4ProtocolManagerUtil::GetRequestUrlAndHeaders(
      req_base64, "threatListUpdates:fetch", config_, gurl, headers);
}

}  // namespace safe_browsing
