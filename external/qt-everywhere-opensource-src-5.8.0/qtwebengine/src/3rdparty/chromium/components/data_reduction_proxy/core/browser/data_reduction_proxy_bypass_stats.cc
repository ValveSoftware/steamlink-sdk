// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_tamper_detection.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

using net::HostPortPair;
using net::ProxyServer;
using net::ProxyService;
using net::NetworkChangeNotifier;
using net::URLRequest;

namespace data_reduction_proxy {

namespace {

const int kMinFailedRequestsWhenUnavailable = 1;
const int kMaxSuccessfulRequestsWhenUnavailable = 0;
const int kMaxFailedRequestsBeforeReset = 3;

// Records a net error code that resulted in bypassing the data reduction
// proxy (|is_primary| is true) or the data reduction proxy fallback.
void RecordDataReductionProxyBypassOnNetworkError(
    bool is_primary,
    const ProxyServer& proxy_server,
    int net_error) {
  if (is_primary) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "DataReductionProxy.BypassOnNetworkErrorPrimary",
        std::abs(net_error));
    return;
  }
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "DataReductionProxy.BypassOnNetworkErrorFallback",
      std::abs(net_error));
}

}  // namespace

// static
void DataReductionProxyBypassStats::RecordDataReductionProxyBypassInfo(
    bool is_primary,
    bool bypass_all,
    const net::ProxyServer& proxy_server,
    DataReductionProxyBypassType bypass_type) {
  if (bypass_all) {
    if (is_primary) {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BlockTypePrimary",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BlockTypeFallback",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    }
  } else {
    if (is_primary) {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BypassTypePrimary",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BypassTypeFallback",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    }
  }
}

// static
void DataReductionProxyBypassStats::DetectAndRecordMissingViaHeaderResponseCode(
    bool is_primary,
    const net::HttpResponseHeaders* headers) {
  if (HasDataReductionProxyViaHeader(headers, NULL)) {
    // The data reduction proxy via header is present, so don't record anything.
    return;
  }

  if (is_primary) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "DataReductionProxy.MissingViaHeader.ResponseCode.Primary",
        headers->response_code());
  } else {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "DataReductionProxy.MissingViaHeader.ResponseCode.Fallback",
        headers->response_code());
  }
}

DataReductionProxyBypassStats::DataReductionProxyBypassStats(
    DataReductionProxyConfig* config,
    UnreachableCallback unreachable_callback)
    : data_reduction_proxy_config_(config),
      unreachable_callback_(unreachable_callback),
      last_bypass_type_(BYPASS_EVENT_TYPE_MAX),
      triggering_request_(true),
      successful_requests_through_proxy_count_(0),
      proxy_net_errors_count_(0),
      unavailable_(false) {
  DCHECK(config);
  NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

DataReductionProxyBypassStats::~DataReductionProxyBypassStats() {
  NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void DataReductionProxyBypassStats::OnUrlRequestCompleted(
    const net::URLRequest* request, bool started) {
  DataReductionProxyTypeInfo proxy_info;
  // Ignore requests that did not use the data reduction proxy. The check for
  // LOAD_BYPASS_PROXY is necessary because the proxy_server() in the |request|
  // might still be set to the data reduction proxy if |request| was retried
  // over direct and a network error occurred while retrying it.
  if (data_reduction_proxy_config_->WasDataReductionProxyUsed(request,
                                                              &proxy_info) &&
      (request->load_flags() & net::LOAD_BYPASS_PROXY) == 0 &&
      request->status().status() == net::URLRequestStatus::SUCCESS) {
    successful_requests_through_proxy_count_++;
    NotifyUnavailabilityIfChanged();

    // Report the success counts.
    UMA_HISTOGRAM_COUNTS_100(
        "DataReductionProxy.SuccessfulRequestCompletionCounts",
        proxy_info.proxy_index);
    if (request->load_flags() & net::LOAD_MAIN_FRAME) {
      UMA_HISTOGRAM_COUNTS_100(
          "DataReductionProxy.SuccessfulRequestCompletionCounts.MainFrame",
          proxy_info.proxy_index);
    }
  }
}

void DataReductionProxyBypassStats::SetBypassType(
    DataReductionProxyBypassType type) {
  last_bypass_type_ = type;
  triggering_request_ = true;
}

DataReductionProxyBypassType
DataReductionProxyBypassStats::GetBypassType() const {
  return last_bypass_type_;
}

void DataReductionProxyBypassStats::RecordBytesHistograms(
    const net::URLRequest& request,
    bool data_reduction_proxy_enabled,
    const net::ProxyConfig& data_reduction_proxy_config) {
  RecordBypassedBytesHistograms(request, data_reduction_proxy_enabled,
                                data_reduction_proxy_config);
  RecordMissingViaHeaderBytes(request);
}

void DataReductionProxyBypassStats::OnProxyFallback(
    const net::ProxyServer& bypassed_proxy,
    int net_error) {
  DataReductionProxyTypeInfo data_reduction_proxy_info;
  if (bypassed_proxy.is_valid() && !bypassed_proxy.is_direct() &&
      data_reduction_proxy_config_->IsDataReductionProxy(
          bypassed_proxy.host_port_pair(), &data_reduction_proxy_info)) {
    proxy_net_errors_count_++;

    // To account for the case when the proxy is reachable for sometime, and
    // then gets blocked, we reset counts when number of errors exceed
    // the threshold.
    if (proxy_net_errors_count_ >= kMaxFailedRequestsBeforeReset &&
        successful_requests_through_proxy_count_ >
            kMaxSuccessfulRequestsWhenUnavailable) {
      ClearRequestCounts();
    } else {
      NotifyUnavailabilityIfChanged();
    }

    if (data_reduction_proxy_info.proxy_index == 0) {
      RecordDataReductionProxyBypassInfo(
          true, false, bypassed_proxy, BYPASS_EVENT_TYPE_NETWORK_ERROR);
      RecordDataReductionProxyBypassOnNetworkError(
          true, bypassed_proxy, net_error);
    } else {
      RecordDataReductionProxyBypassInfo(
          false, false, bypassed_proxy, BYPASS_EVENT_TYPE_NETWORK_ERROR);
      RecordDataReductionProxyBypassOnNetworkError(
          false, bypassed_proxy, net_error);
    }
  }
}

void DataReductionProxyBypassStats::ClearRequestCounts() {
  successful_requests_through_proxy_count_ = 0;
  proxy_net_errors_count_ = 0;
}

void DataReductionProxyBypassStats::NotifyUnavailabilityIfChanged() {
  bool prev_unavailable = unavailable_;
  unavailable_ =
      (proxy_net_errors_count_ >= kMinFailedRequestsWhenUnavailable &&
       successful_requests_through_proxy_count_ <=
           kMaxSuccessfulRequestsWhenUnavailable);
  if (prev_unavailable != unavailable_)
    unreachable_callback_.Run(unavailable_);
}

void DataReductionProxyBypassStats::RecordBypassedBytesHistograms(
    const net::URLRequest& request,
    bool data_reduction_proxy_enabled,
    const net::ProxyConfig& data_reduction_proxy_config) {
  int64_t content_length = request.received_response_content_length();

  // Only record histograms when the data reduction proxy is enabled.
  if (!data_reduction_proxy_enabled)
    return;

  // TODO(bengr): Add histogram(s) for byte counts of unsupported schemes, e.g.,
  // ws and wss.
  if (!request.url().SchemeIsHTTPOrHTTPS())
    return;

  DataReductionProxyTypeInfo data_reduction_proxy_type_info;
  if (data_reduction_proxy_config_->WasDataReductionProxyUsed(
          &request, &data_reduction_proxy_type_info)) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyBypassStats::NOT_BYPASSED,
                        content_length);

    if (data_reduction_proxy_type_info.proxy_servers.empty())
      return;

    // Obtain the proxy that this request used.
    const net::ProxyServer& proxy =
        data_reduction_proxy_type_info.proxy_servers.front();
    if (proxy.is_valid() && !proxy.host_port_pair().IsEmpty()) {
      DataReductionProxyTamperDetection::DetectAndReport(
          request.response_info().headers.get(),
          proxy.is_https() || proxy.is_quic(), content_length);
    }
    return;
  }

  if (request.url().SchemeIs(url::kHttpsScheme)) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyBypassStats::SSL,
                        content_length);
    return;
  }

  // Now that the data reduction proxy is a best effort proxy, if the effective
  // proxy configuration resolves to anything other than direct:// for a URL,
  // the data reduction proxy will not be used.
  DCHECK(data_reduction_proxy_type_info.proxy_servers.empty());
  if (!request.proxy_server().IsEmpty()) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyBypassStats::PROXY_OVERRIDDEN,
                        content_length);
    return;
  }

  if (data_reduction_proxy_config_->IsBypassedByDataReductionProxyLocalRules(
          request, data_reduction_proxy_config)) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyBypassStats::LOCAL_BYPASS_RULES,
                        content_length);
    return;
  }

  std::string mime_type;
  if (request.response_headers())
    request.response_headers()->GetMimeType(&mime_type);
  // MIME types are named by <media-type>/<subtype>. Check to see if the media
  // type is audio or video in order to record audio/video bypasses separately
  // for current bypasses and for the triggering requests of short bypasses.
  if ((last_bypass_type_ == BYPASS_EVENT_TYPE_CURRENT ||
       (triggering_request_ && last_bypass_type_ == BYPASS_EVENT_TYPE_SHORT)) &&
      (mime_type.compare(0, 6, "audio/") == 0 ||
       mime_type.compare(0, 6, "video/") == 0)) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyBypassStats::AUDIO_VIDEO,
                        content_length);
    triggering_request_ = false;
    return;
  }

  // Report current bypasses of MIME type "application/octet-stream" separately.
  if (last_bypass_type_ == BYPASS_EVENT_TYPE_CURRENT &&
      mime_type.find("application/octet-stream") != std::string::npos) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyBypassStats::APPLICATION_OCTET_STREAM,
                        content_length);
    return;
  }

  // Only record separate triggering request UMA for short, medium, and long
  // bypass events.
  if (triggering_request_ &&
     (last_bypass_type_ == BYPASS_EVENT_TYPE_SHORT ||
      last_bypass_type_ == BYPASS_EVENT_TYPE_MEDIUM ||
      last_bypass_type_ == BYPASS_EVENT_TYPE_LONG)) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyBypassStats::TRIGGERING_REQUEST,
                        content_length);
    triggering_request_ = false;
    return;
  }

  if (last_bypass_type_ != BYPASS_EVENT_TYPE_MAX) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyBypassStats::BYPASSED_BYTES_TYPE_MAX,
                        content_length);
    return;
  }

  if (data_reduction_proxy_config_->AreDataReductionProxiesBypassed(
          request, data_reduction_proxy_config, NULL)) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyBypassStats::NETWORK_ERROR,
                        content_length);
  }
}

void DataReductionProxyBypassStats::RecordMissingViaHeaderBytes(
    const URLRequest& request) {
  // Responses that were served from cache should have been filtered out
  // already.
  DCHECK(!request.was_cached());

  if (!data_reduction_proxy_config_->WasDataReductionProxyUsed(&request,
                                                               NULL) ||
      HasDataReductionProxyViaHeader(request.response_headers(), NULL)) {
    // Only track requests that used the data reduction proxy and had responses
    // that were missing the data reduction proxy via header.
    return;
  }

  if (request.GetResponseCode() >= net::HTTP_BAD_REQUEST &&
      request.GetResponseCode() < net::HTTP_INTERNAL_SERVER_ERROR) {
    // Track 4xx responses that are missing via headers separately.
    UMA_HISTOGRAM_COUNTS("DataReductionProxy.MissingViaHeader.Bytes.4xx",
                         request.received_response_content_length());
  } else {
    UMA_HISTOGRAM_COUNTS("DataReductionProxy.MissingViaHeader.Bytes.Other",
                         request.received_response_content_length());
  }
}

void DataReductionProxyBypassStats::OnNetworkChanged(
    NetworkChangeNotifier::ConnectionType type) {
  ClearRequestCounts();
}

void DataReductionProxyBypassStats::RecordBypassedBytes(
    DataReductionProxyBypassType bypass_type,
    DataReductionProxyBypassStats::BypassedBytesType bypassed_bytes_type,
    int64_t content_length) {
  // Individual histograms are needed to count the bypassed bytes for each
  // bypass type so that we can see the size of requests. This helps us
  // remove outliers that would skew the sum of bypassed bytes for each type.
  switch (bypassed_bytes_type) {
    case DataReductionProxyBypassStats::NOT_BYPASSED:
      UMA_HISTOGRAM_COUNTS(
          "DataReductionProxy.BypassedBytes.NotBypassed", content_length);
      break;
    case DataReductionProxyBypassStats::SSL:
      UMA_HISTOGRAM_COUNTS(
          "DataReductionProxy.BypassedBytes.SSL", content_length);
      break;
    case DataReductionProxyBypassStats::LOCAL_BYPASS_RULES:
      UMA_HISTOGRAM_COUNTS(
          "DataReductionProxy.BypassedBytes.LocalBypassRules",
          content_length);
      break;
    case DataReductionProxyBypassStats::PROXY_OVERRIDDEN:
      UMA_HISTOGRAM_COUNTS(
          "DataReductionProxy.BypassedBytes.ProxyOverridden",
          content_length);
      break;
    case DataReductionProxyBypassStats::AUDIO_VIDEO:
      switch (bypass_type) {
        case BYPASS_EVENT_TYPE_CURRENT:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.CurrentAudioVideo",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_SHORT:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.ShortAudioVideo",
              content_length);
          break;
        default:
          NOTREACHED();
      }
      break;
    case DataReductionProxyBypassStats::APPLICATION_OCTET_STREAM:
      switch (bypass_type) {
        case BYPASS_EVENT_TYPE_CURRENT:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.CurrentApplicationOctetStream",
              content_length);
          break;
        default:
          NOTREACHED();
      }
      break;
    case DataReductionProxyBypassStats::TRIGGERING_REQUEST:
      switch (bypass_type) {
        case BYPASS_EVENT_TYPE_SHORT:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.ShortTriggeringRequest",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_MEDIUM:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.MediumTriggeringRequest",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_LONG:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.LongTriggeringRequest",
              content_length);
          break;
        default:
          break;
      }
      break;
    case DataReductionProxyBypassStats::NETWORK_ERROR:
      UMA_HISTOGRAM_COUNTS(
          "DataReductionProxy.BypassedBytes.NetworkErrorOther",
          content_length);
      break;
    case DataReductionProxyBypassStats::BYPASSED_BYTES_TYPE_MAX:
      switch (bypass_type) {
        case BYPASS_EVENT_TYPE_CURRENT:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.Current",
                               content_length);
          break;
        case BYPASS_EVENT_TYPE_SHORT:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.ShortAll",
                               content_length);
          break;
        case BYPASS_EVENT_TYPE_MEDIUM:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.MediumAll",
                               content_length);
          break;
        case BYPASS_EVENT_TYPE_LONG:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.LongAll",
                               content_length);
          break;
        case BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_4XX:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.MissingViaHeader4xx",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.MissingViaHeaderOther",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_MALFORMED_407:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.Malformed407",
                               content_length);
          break;
        case BYPASS_EVENT_TYPE_STATUS_500_HTTP_INTERNAL_SERVER_ERROR:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes."
              "Status500HttpInternalServerError",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.Status502HttpBadGateway",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_STATUS_503_HTTP_SERVICE_UNAVAILABLE:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes."
              "Status503HttpServiceUnavailable",
              content_length);
          break;
        default:
          break;
      }
      break;
  }
}

}  // namespace data_reduction_proxy
