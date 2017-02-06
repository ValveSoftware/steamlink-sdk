// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"

#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/data_reduction_proxy/proto/pageload_metrics.pb.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

static const char kHistogramSucceeded[] =
    "DataReductionProxy.Pingback.Succeeded";
static const char kHistogramAttempted[] =
    "DataReductionProxy.Pingback.Attempted";

// Creates a PageloadMetrics protobuf for this page load and serializes to a
// string.
std::string SerializeData(const DataReductionProxyData& request_data,
                          const DataReductionProxyPageLoadTiming& timing) {
  RecordPageloadMetricsRequest batched_request;
  PageloadMetrics* request = batched_request.add_pageloads();
  request->set_session_key(request_data.session_key());
  // For the timing events, any of them could be zero. Fill the message as a
  // best effort.
  request->set_allocated_first_request_time(
      protobuf_parser::CreateTimestampFromTime(timing.navigation_start)
          .release());
  if (request_data.original_request_url().is_valid())
    request->set_first_request_url(request_data.original_request_url().spec());
  if (!timing.first_contentful_paint.is_zero()) {
    request->set_allocated_time_to_first_contentful_paint(
        protobuf_parser::CreateDurationFromTimeDelta(
            timing.first_contentful_paint)
            .release());
  }
  if (!timing.first_image_paint.is_zero()) {
    request->set_allocated_time_to_first_image_paint(
        protobuf_parser::CreateDurationFromTimeDelta(timing.first_image_paint)
            .release());
  }
  if (!timing.response_start.is_zero()) {
    request->set_allocated_time_to_first_byte(
        protobuf_parser::CreateDurationFromTimeDelta(timing.response_start)
            .release());
  }
  if (!timing.load_event_start.is_zero()) {
    request->set_allocated_page_load_time(
        protobuf_parser::CreateDurationFromTimeDelta(timing.load_event_start)
            .release());
  }
  std::string serialized_request;
  batched_request.SerializeToString(&serialized_request);
  return serialized_request;
}

}  // namespace

DataReductionProxyPingbackClient::DataReductionProxyPingbackClient(
    net::URLRequestContextGetter* url_request_context)
    : url_request_context_(url_request_context),
      pingback_url_(util::AddApiKeyToUrl(params::GetPingbackURL())),
      pingback_reporting_fraction_(0.0) {}

DataReductionProxyPingbackClient::~DataReductionProxyPingbackClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DataReductionProxyPingbackClient::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(source == current_fetcher_.get());
  UMA_HISTOGRAM_BOOLEAN(kHistogramSucceeded, source->GetStatus().is_success());
  current_fetcher_.reset();
  while (!current_fetcher_ && !data_to_send_.empty()) {
    current_fetcher_ = MaybeCreateFetcherForDataAndStart(data_to_send_.front());
    data_to_send_.pop_front();
  }
}

void DataReductionProxyPingbackClient::SendPingback(
    const DataReductionProxyData& request_data,
    const DataReductionProxyPageLoadTiming& timing) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string serialized_request = SerializeData(request_data, timing);
  if (current_fetcher_.get()) {
    data_to_send_.push_back(serialized_request);
  } else {
    DCHECK(data_to_send_.empty());
    current_fetcher_ = MaybeCreateFetcherForDataAndStart(serialized_request);
  }
}

std::unique_ptr<net::URLFetcher>
DataReductionProxyPingbackClient::MaybeCreateFetcherForDataAndStart(
    const std::string& request_data) {
  bool send_pingback = ShouldSendPingback();
  UMA_HISTOGRAM_BOOLEAN(kHistogramAttempted, send_pingback);
  if (!send_pingback)
    return nullptr;
  std::unique_ptr<net::URLFetcher> fetcher(
      net::URLFetcher::Create(pingback_url_, net::URLFetcher::POST, this));
  fetcher->SetLoadFlags(net::LOAD_BYPASS_PROXY);
  fetcher->SetUploadData("application/x-protobuf", request_data);
  fetcher->SetRequestContext(url_request_context_);
  // Configure max retries to be at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  fetcher->SetMaxRetriesOn5xx(kMaxRetries);
  fetcher->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);
  fetcher->Start();
  return fetcher;
}

bool DataReductionProxyPingbackClient::ShouldSendPingback() const {
  return params::IsForcePingbackEnabledViaFlags() ||
         GenerateRandomFloat() < pingback_reporting_fraction_;
}

float DataReductionProxyPingbackClient::GenerateRandomFloat() const {
  return static_cast<float>(base::RandDouble());
}

void DataReductionProxyPingbackClient::SetPingbackReportingFraction(
    float pingback_reporting_fraction) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(0.0f, pingback_reporting_fraction);
  DCHECK_GE(1.0f, pingback_reporting_fraction);
  pingback_reporting_fraction_ = pingback_reporting_fraction;
}

}  // namespace data_reduction_proxy
