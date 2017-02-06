// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"

#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_list.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

// Adds non-empty entries in |data_reduction_proxies| to the retry map
// maintained by the proxy service of the request. Adds
// |data_reduction_proxies.second| to the retry list only if |bypass_all| is
// true.
void MarkProxiesAsBadUntil(
    net::URLRequest* request,
    const base::TimeDelta& bypass_duration,
    bool bypass_all,
    const std::vector<net::ProxyServer>& data_reduction_proxies) {
  DCHECK(!data_reduction_proxies.empty());
  // Synthesize a suitable |ProxyInfo| to add the proxies to the
  // |ProxyRetryInfoMap| of the proxy service.
  net::ProxyList proxy_list;
  std::vector<net::ProxyServer> additional_bad_proxies;
  for (const net::ProxyServer& proxy_server : data_reduction_proxies) {
    DCHECK(proxy_server.is_valid());
    proxy_list.AddProxyServer(proxy_server);
    if (!bypass_all)
      break;
    additional_bad_proxies.push_back(proxy_server);
  }
  proxy_list.AddProxyServer(net::ProxyServer::Direct());

  net::ProxyInfo proxy_info;
  proxy_info.UseProxyList(proxy_list);
  DCHECK(request->context());
  net::ProxyService* proxy_service = request->context()->proxy_service();
  DCHECK(proxy_service);

  proxy_service->MarkProxiesAsBadUntil(
      proxy_info, bypass_duration, additional_bad_proxies, request->net_log());
}

void ReportResponseProxyServerStatusHistogram(
    DataReductionProxyBypassProtocol::ResponseProxyServerStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "DataReductionProxy.ResponseProxyServerStatus", status,
      DataReductionProxyBypassProtocol::RESPONSE_PROXY_SERVER_STATUS_MAX);
}

}  // namespace

DataReductionProxyBypassProtocol::DataReductionProxyBypassProtocol(
    DataReductionProxyConfig* config)
    : config_(config) {
  DCHECK(config_);
}

DataReductionProxyBypassProtocol::~DataReductionProxyBypassProtocol() {
}

bool DataReductionProxyBypassProtocol::MaybeBypassProxyAndPrepareToRetry(
    net::URLRequest* request,
    DataReductionProxyBypassType* proxy_bypass_type,
    DataReductionProxyInfo* data_reduction_proxy_info) {
  DCHECK(request);
  const net::HttpResponseHeaders* response_headers =
      request->response_info().headers.get();
  if (!response_headers)
    return false;

  // Empty implies either that the request was served from cache or that
  // request was served directly from the origin.
  if (request->proxy_server().IsEmpty()) {
    ReportResponseProxyServerStatusHistogram(
        RESPONSE_PROXY_SERVER_STATUS_EMPTY);
    return false;
  }

  DataReductionProxyTypeInfo data_reduction_proxy_type_info;
  if (!config_->WasDataReductionProxyUsed(request,
                                          &data_reduction_proxy_type_info)) {
    if (!HasDataReductionProxyViaHeader(response_headers, nullptr)) {
      ReportResponseProxyServerStatusHistogram(
          RESPONSE_PROXY_SERVER_STATUS_NON_DRP_NO_VIA);
      return false;
    }
    ReportResponseProxyServerStatusHistogram(
        RESPONSE_PROXY_SERVER_STATUS_NON_DRP_WITH_VIA);

    // If the |proxy_server| doesn't match any of the currently configured Data
    // Reduction Proxies, but it still has the Data Reduction Proxy via header,
    // then apply the bypass logic regardless.
    // TODO(sclittle): Remove this workaround once http://crbug.com/476610 is
    // fixed.
    data_reduction_proxy_type_info.proxy_servers.push_back(net::ProxyServer(
        net::ProxyServer::SCHEME_HTTPS, request->proxy_server()));
    data_reduction_proxy_type_info.proxy_servers.push_back(net::ProxyServer(
        net::ProxyServer::SCHEME_HTTP, request->proxy_server()));
    data_reduction_proxy_type_info.proxy_index = 0;
  } else {
    ReportResponseProxyServerStatusHistogram(RESPONSE_PROXY_SERVER_STATUS_DRP);
  }

  if (data_reduction_proxy_type_info.proxy_servers.empty())
    return false;

  // At this point, the response is expected to have the data reduction proxy
  // via header, so detect and report cases where the via header is missing.
  DataReductionProxyBypassStats::DetectAndRecordMissingViaHeaderResponseCode(
      data_reduction_proxy_type_info.proxy_index == 0, response_headers);

  // GetDataReductionProxyBypassType will only log a net_log event if a bypass
  // command was sent via the data reduction proxy headers
  DataReductionProxyBypassType bypass_type = GetDataReductionProxyBypassType(
      response_headers, data_reduction_proxy_info);

  if (proxy_bypass_type)
    *proxy_bypass_type = bypass_type;
  if (bypass_type == BYPASS_EVENT_TYPE_MAX)
    return false;

  DCHECK(request->context());
  DCHECK(request->context()->proxy_service());
  net::ProxyServer proxy_server =
      data_reduction_proxy_type_info.proxy_servers.front();

  // Only record UMA if the proxy isn't already on the retry list.
  if (!config_->IsProxyBypassed(
          request->context()->proxy_service()->proxy_retry_info(), proxy_server,
          NULL)) {
    DataReductionProxyBypassStats::RecordDataReductionProxyBypassInfo(
        data_reduction_proxy_type_info.proxy_index == 0,
        data_reduction_proxy_info->bypass_all, proxy_server, bypass_type);
  }

  if (data_reduction_proxy_info->mark_proxies_as_bad) {
    MarkProxiesAsBadUntil(request, data_reduction_proxy_info->bypass_duration,
                          data_reduction_proxy_info->bypass_all,
                          data_reduction_proxy_type_info.proxy_servers);
  } else {
    request->SetLoadFlags(request->load_flags() |
                          net::LOAD_DISABLE_CACHE |
                          net::LOAD_BYPASS_PROXY);
  }

  // Retry if block-once was specified or if method is idempotent.
  return bypass_type == BYPASS_EVENT_TYPE_CURRENT ||
         util::IsMethodIdempotent(request->method());
}

}  // namespace data_reduction_proxy
