// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_use_group.h"
#include "components/data_reduction_proxy/core/browser/data_use_group_provider.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/lofi_decider.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace {

// |lofi_low_header_added| is set to true iff Lo-Fi "q=low" request header can
// be added to the Chrome proxy headers.
// |received_content_length| is the number of prefilter bytes received.
// |original_content_length| is the length of resource if accessed directly
// without data saver proxy.
// |freshness_lifetime| contains information on how long the resource will be
// fresh for and how long is the usability.
void RecordContentLengthHistograms(bool lofi_low_header_added,
                                   int64_t received_content_length,
                                   int64_t original_content_length,
                                   const base::TimeDelta& freshness_lifetime) {
  // Add the current resource to these histograms only when a valid
  // X-Original-Content-Length header is present.
  if (original_content_length >= 0) {
    UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthWithValidOCL",
                         received_content_length);
    UMA_HISTOGRAM_COUNTS("Net.HttpOriginalContentLengthWithValidOCL",
                         original_content_length);
    UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthDifferenceWithValidOCL",
                         original_content_length - received_content_length);

    // Populate Lo-Fi content length histograms.
    if (lofi_low_header_added) {
      UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthWithValidOCL.LoFiOn",
                           received_content_length);
      UMA_HISTOGRAM_COUNTS("Net.HttpOriginalContentLengthWithValidOCL.LoFiOn",
                           original_content_length);
      UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthDifferenceWithValidOCL.LoFiOn",
                           original_content_length - received_content_length);
    }

  } else {
    // Presume the original content length is the same as the received content
    // length if the X-Original-Content-Header is not present.
    original_content_length = received_content_length;
  }
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLength", received_content_length);
  UMA_HISTOGRAM_COUNTS("Net.HttpOriginalContentLength",
                       original_content_length);
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthDifference",
                       original_content_length - received_content_length);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.HttpContentFreshnessLifetime",
                              freshness_lifetime.InSeconds(),
                              base::TimeDelta::FromHours(1).InSeconds(),
                              base::TimeDelta::FromDays(30).InSeconds(),
                              100);
  if (freshness_lifetime.InSeconds() <= 0)
    return;
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthCacheable",
                       received_content_length);
  if (freshness_lifetime.InHours() < 4)
    return;
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthCacheable4Hours",
                       received_content_length);

  if (freshness_lifetime.InHours() < 24)
    return;
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthCacheable24Hours",
                       received_content_length);
}

// Given a |request| that went through the Data Reduction Proxy, this function
// estimates how many bytes would have been received if the response had been
// received directly from the origin using HTTP/1.1 with a content length of
// |adjusted_original_content_length|.
int64_t EstimateOriginalReceivedBytes(
    const net::URLRequest& request,
    int64_t adjusted_original_content_length) {
  DCHECK_LE(0, adjusted_original_content_length);

  if (!request.status().is_success() || request.was_cached() ||
      !request.response_headers()) {
    return request.GetTotalReceivedBytes();
  }

  // TODO(sclittle): Remove headers added by Data Reduction Proxy when computing
  // original size. http://crbug/535701.
  return request.response_headers()->raw_headers().size() +
         adjusted_original_content_length;
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxyNetworkDelegate::DataReductionProxyNetworkDelegate(
    std::unique_ptr<net::NetworkDelegate> network_delegate,
    DataReductionProxyConfig* config,
    DataReductionProxyRequestOptions* request_options,
    const DataReductionProxyConfigurator* configurator)
    : LayeredNetworkDelegate(std::move(network_delegate)),
      total_received_bytes_(0),
      total_original_received_bytes_(0),
      data_reduction_proxy_config_(config),
      data_reduction_proxy_bypass_stats_(nullptr),
      data_reduction_proxy_request_options_(request_options),
      data_reduction_proxy_io_data_(nullptr),
      configurator_(configurator) {
  DCHECK(data_reduction_proxy_config_);
  DCHECK(data_reduction_proxy_request_options_);
  DCHECK(configurator_);
}

DataReductionProxyNetworkDelegate::~DataReductionProxyNetworkDelegate() {
}

void DataReductionProxyNetworkDelegate::InitIODataAndUMA(
    DataReductionProxyIOData* io_data,
    DataReductionProxyBypassStats* bypass_stats) {
  DCHECK(bypass_stats);
  data_reduction_proxy_io_data_ = io_data;
  data_reduction_proxy_bypass_stats_ = bypass_stats;
}

base::Value*
DataReductionProxyNetworkDelegate::SessionNetworkStatsInfoToValue() const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  // Use strings to avoid overflow. base::Value only supports 32-bit integers.
  dict->SetString("session_received_content_length",
                  base::Int64ToString(total_received_bytes_));
  dict->SetString("session_original_content_length",
                  base::Int64ToString(total_original_received_bytes_));
  return dict;
}

void DataReductionProxyNetworkDelegate::OnBeforeURLRequestInternal(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  if (data_use_group_provider_) {
    // Creates and initializes a |DataUseGroup| for the |request| if it does not
    // exist. Even though we do not use the |DataUseGroup| here, we want to
    // associate one with a request as early as possible in case the frame
    // associated with the request goes away before the request is completed.
    scoped_refptr<DataUseGroup> data_use_group =
        data_use_group_provider_->GetDataUseGroup(request);
    data_use_group->Initialize();
  }

  // |data_reduction_proxy_io_data_| can be NULL for Webview.
  if (data_reduction_proxy_io_data_ &&
      (request->load_flags() & net::LOAD_MAIN_FRAME)) {
    data_reduction_proxy_io_data_->SetLoFiModeActiveOnMainFrame(false);
  }
}

void DataReductionProxyNetworkDelegate::OnBeforeSendHeadersInternal(
    net::URLRequest* request,
    const net::ProxyInfo& proxy_info,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::HttpRequestHeaders* headers) {
  DCHECK(data_reduction_proxy_config_);
  DCHECK(request);
  if (params::IsIncludedInHoldbackFieldTrial()) {
    if (!WasEligibleWithoutHoldback(*request, proxy_info, proxy_retry_info))
      return;
    // For the holdback field trial, still log UMA as if the proxy was used.
    DataReductionProxyData* data =
        DataReductionProxyData::GetDataAndCreateIfNecessary(request);
    if (data)
      data->set_used_data_reduction_proxy(true);
    return;
  }

  // The following checks rule out direct, invalid, and othe connection types.
  if (!proxy_info.is_http() && !proxy_info.is_https() && !proxy_info.is_quic())
    return;
  if (proxy_info.proxy_server().host_port_pair().IsEmpty())
    return;
  if (!data_reduction_proxy_config_->IsDataReductionProxy(
          proxy_info.proxy_server().host_port_pair(), nullptr)) {
    return;
  }

  // Retrieves DataReductionProxyData from a request, creating a new instance
  // if needed.
  DataReductionProxyData* data =
      DataReductionProxyData::GetDataAndCreateIfNecessary(request);
  if (data) {
    data->set_used_data_reduction_proxy(true);
    data->set_session_key(
        data_reduction_proxy_request_options_->GetSecureSession());
    data->set_original_request_url(request->original_url());
  }

  if (data_reduction_proxy_io_data_ &&
      data_reduction_proxy_io_data_->lofi_decider()) {
    LoFiDecider* lofi_decider = data_reduction_proxy_io_data_->lofi_decider();
    const bool is_using_lofi_mode =
        lofi_decider->MaybeAddLoFiDirectiveToHeaders(*request, headers);

    if ((request->load_flags() & net::LOAD_MAIN_FRAME)) {
      data_reduction_proxy_io_data_->SetLoFiModeActiveOnMainFrame(
          is_using_lofi_mode);
    }

    if (data)
      data->set_lofi_requested(lofi_decider->ShouldRecordLoFiUMA(*request));
  }

  if (data_reduction_proxy_request_options_) {
    data_reduction_proxy_request_options_->AddRequestHeader(headers);
  }
}

void DataReductionProxyNetworkDelegate::OnCompletedInternal(
    net::URLRequest* request,
    bool started) {
  DCHECK(request);
  if (data_reduction_proxy_bypass_stats_)
    data_reduction_proxy_bypass_stats_->OnUrlRequestCompleted(request, started);

  net::HttpRequestHeaders request_headers;
  if (data_reduction_proxy_io_data_ && request->response_headers() &&
      request->response_headers()->HasHeaderValue(
          chrome_proxy_header(), chrome_proxy_lo_fi_directive())) {
    data_reduction_proxy_io_data_->lofi_ui_service()->OnLoFiReponseReceived(
        *request, false);
  } else if (data_reduction_proxy_io_data_ && request->response_headers() &&
             request->response_headers()->HasHeaderValue(
                 chrome_proxy_header(),
                 chrome_proxy_lo_fi_preview_directive())) {
    data_reduction_proxy_io_data_->lofi_ui_service()->OnLoFiReponseReceived(
        *request, true);
    RecordLoFiTransformationType(PREVIEW);
  } else if (request->GetFullRequestHeaders(&request_headers) &&
             request_headers.HasHeader(chrome_proxy_header())) {
    std::string header_value;
    request_headers.GetHeader(chrome_proxy_header(), &header_value);
    if (header_value.find(chrome_proxy_lo_fi_preview_directive()) !=
        std::string::npos) {
      RecordLoFiTransformationType(NO_TRANSFORMATION_PREVIEW_REQUESTED);
    }
  }

  if (!request->response_info().network_accessed ||
      !request->url().SchemeIsHTTPOrHTTPS() ||
      request->GetTotalReceivedBytes() == 0) {
    return;
  }

  DataReductionProxyRequestType request_type = GetDataReductionProxyRequestType(
      *request, configurator_->GetProxyConfig(), *data_reduction_proxy_config_);

  // Determine the original content length if present.
  int64_t original_content_length =
      request->response_headers()
          ? request->response_headers()->GetInt64HeaderValue(
                "x-original-content-length")
          : -1;

  CalculateAndRecordDataUsage(*request, request_type, original_content_length);

  RecordContentLength(*request, request_type, original_content_length);
}

void DataReductionProxyNetworkDelegate::CalculateAndRecordDataUsage(
    const net::URLRequest& request,
    DataReductionProxyRequestType request_type,
    int64_t original_content_length) {
  DCHECK_LE(-1, original_content_length);
  int64_t data_used = request.GetTotalReceivedBytes();

  // Estimate how many bytes would have been used if the DataReductionProxy was
  // not used, and record the data usage.
  int64_t original_size = data_used;
  if (request_type == VIA_DATA_REDUCTION_PROXY && request.response_headers() &&
      !request.was_cached() && request.status().is_success()) {
    original_size = EstimateOriginalReceivedBytes(
        request, GetAdjustedOriginalContentLength(
                     request_type, original_content_length,
                     request.received_response_content_length()));
  }

  std::string mime_type;
  if (request.response_headers())
    request.response_headers()->GetMimeType(&mime_type);

  scoped_refptr<DataUseGroup> data_use_group =
      data_use_group_provider_
          ? data_use_group_provider_->GetDataUseGroup(&request)
          : nullptr;
  AccumulateDataUsage(data_used, original_size, request_type, data_use_group,
                      mime_type);
}

void DataReductionProxyNetworkDelegate::AccumulateDataUsage(
    int64_t data_used,
    int64_t original_size,
    DataReductionProxyRequestType request_type,
    const scoped_refptr<DataUseGroup>& data_use_group,
    const std::string& mime_type) {
  DCHECK_GE(data_used, 0);
  DCHECK_GE(original_size, 0);
  if (data_reduction_proxy_io_data_) {
    data_reduction_proxy_io_data_->UpdateContentLengths(
        data_used, original_size, data_reduction_proxy_io_data_->IsEnabled(),
        request_type, data_use_group, mime_type);
  }
  total_received_bytes_ += data_used;
  total_original_received_bytes_ += original_size;
}

void DataReductionProxyNetworkDelegate::RecordContentLength(
    const net::URLRequest& request,
    DataReductionProxyRequestType request_type,
    int64_t original_content_length) {
  if (!request.response_headers() || request.was_cached() ||
      request.received_response_content_length() == 0) {
    return;
  }

  // Record content length histograms for the request.
  base::TimeDelta freshness_lifetime =
      request.response_headers()
          ->GetFreshnessLifetimes(request.response_info().response_time)
          .freshness;

  RecordContentLengthHistograms(
      // |data_reduction_proxy_io_data_| can be NULL for Webview.
      data_reduction_proxy_io_data_ &&
          data_reduction_proxy_io_data_->IsEnabled() &&
          data_reduction_proxy_io_data_->lofi_decider() &&
          data_reduction_proxy_io_data_->lofi_decider()->IsUsingLoFiMode(
              request),
      request.received_response_content_length(), original_content_length,
      freshness_lifetime);

  if (data_reduction_proxy_io_data_ && data_reduction_proxy_bypass_stats_) {
    // Record BypassedBytes histograms for the request.
    data_reduction_proxy_bypass_stats_->RecordBytesHistograms(
        request, data_reduction_proxy_io_data_->IsEnabled(),
        configurator_->GetProxyConfig());
  }
}

void DataReductionProxyNetworkDelegate::RecordLoFiTransformationType(
    LoFiTransformationType type) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.LoFi.TransformationType", type,
                            LO_FI_TRANSFORMATION_TYPES_INDEX_BOUNDARY);
}

bool DataReductionProxyNetworkDelegate::WasEligibleWithoutHoldback(
    const net::URLRequest& request,
    const net::ProxyInfo& proxy_info,
    const net::ProxyRetryInfoMap& proxy_retry_info) const {
  DCHECK(proxy_info.is_empty() || proxy_info.is_direct() ||
         !data_reduction_proxy_config_->IsDataReductionProxy(
             proxy_info.proxy_server().host_port_pair(), nullptr));
  if (!util::EligibleForDataReductionProxy(proxy_info, request.url(),
                                           request.method())) {
    return false;
  }
  net::ProxyConfig proxy_config =
      data_reduction_proxy_config_->ProxyConfigIgnoringHoldback();
  net::ProxyInfo data_reduction_proxy_info;
  return util::ApplyProxyConfigToProxyInfo(proxy_config, proxy_retry_info,
                                           request.url(),
                                           &data_reduction_proxy_info);
}

void DataReductionProxyNetworkDelegate::SetDataUseGroupProvider(
    std::unique_ptr<DataUseGroupProvider> data_use_group_provider) {
  data_use_group_provider_.reset(data_use_group_provider.release());
}

}  // namespace data_reduction_proxy
