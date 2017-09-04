// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"

#include <cmath>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_service.h"

namespace data_reduction_proxy {

namespace {
static const char kDataReductionCoreProxy[] = "proxy.googlezip.net";
}

DataReductionProxyDelegate::DataReductionProxyDelegate(
    DataReductionProxyConfig* config,
    const DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator,
    DataReductionProxyBypassStats* bypass_stats,
    net::NetLog* net_log)
    : config_(config),
      configurator_(configurator),
      event_creator_(event_creator),
      bypass_stats_(bypass_stats),
      alternative_proxies_broken_(false),
      net_log_(net_log) {
  DCHECK(config);
  DCHECK(configurator);
  DCHECK(event_creator);
  DCHECK(bypass_stats);
  DCHECK(net_log);
}

DataReductionProxyDelegate::~DataReductionProxyDelegate() {
}

void DataReductionProxyDelegate::OnResolveProxy(
    const GURL& url,
    const std::string& method,
    const net::ProxyService& proxy_service,
    net::ProxyInfo* result) {
  DCHECK(result);
  OnResolveProxyHandler(url, method, configurator_->GetProxyConfig(),
                        proxy_service.proxy_retry_info(), config_, result);
}

void DataReductionProxyDelegate::OnTunnelConnectCompleted(
    const net::HostPortPair& endpoint,
    const net::HostPortPair& proxy_server,
    int net_error) {
}

void DataReductionProxyDelegate::OnFallback(const net::ProxyServer& bad_proxy,
                                            int net_error) {
  if (bad_proxy.is_valid() &&
      config_->IsDataReductionProxy(bad_proxy, nullptr)) {
    event_creator_->AddProxyFallbackEvent(net_log_, bad_proxy.ToURI(),
                                          net_error);
  }

  if (bypass_stats_)
    bypass_stats_->OnProxyFallback(bad_proxy, net_error);
}

void DataReductionProxyDelegate::OnBeforeTunnelRequest(
    const net::HostPortPair& proxy_server,
    net::HttpRequestHeaders* extra_headers) {
}

bool DataReductionProxyDelegate::IsTrustedSpdyProxy(
    const net::ProxyServer& proxy_server) {
  if (!proxy_server.is_https() ||
      !params::IsIncludedInTrustedSpdyProxyFieldTrial() ||
      !proxy_server.is_valid()) {
    return false;
  }
  return config_ && config_->IsDataReductionProxy(proxy_server, nullptr);
}

void DataReductionProxyDelegate::OnTunnelHeadersReceived(
    const net::HostPortPair& origin,
    const net::HostPortPair& proxy_server,
    const net::HttpResponseHeaders& response_headers) {
}

void DataReductionProxyDelegate::GetAlternativeProxy(
    const GURL& url,
    const net::ProxyServer& resolved_proxy_server,
    net::ProxyServer* alternative_proxy_server) const {
  DCHECK(!alternative_proxy_server->is_valid());

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() ||
      url.SchemeIsCryptographic()) {
    return;
  }

  if (!params::IsIncludedInQuicFieldTrial())
    return;

  if (!resolved_proxy_server.is_valid() || !resolved_proxy_server.is_https())
    return;

  if (!config_ ||
      !config_->IsDataReductionProxy(resolved_proxy_server, nullptr)) {
    return;
  }

  if (alternative_proxies_broken_) {
    RecordQuicProxyStatus(QUIC_PROXY_STATUS_MARKED_AS_BROKEN);
    return;
  }

  if (!SupportsQUIC(resolved_proxy_server)) {
    RecordQuicProxyStatus(QUIC_PROXY_NOT_SUPPORTED);
    return;
  }

  *alternative_proxy_server = net::ProxyServer(
      net::ProxyServer::SCHEME_QUIC, resolved_proxy_server.host_port_pair());
  DCHECK(alternative_proxy_server->is_valid());
  RecordQuicProxyStatus(QUIC_PROXY_STATUS_AVAILABLE);
  return;
}

void DataReductionProxyDelegate::OnAlternativeProxyBroken(
    const net::ProxyServer& alternative_proxy_server) {
  // TODO(tbansal): Reset this on connection change events.
  // Currently, DataReductionProxyDelegate does not maintain a list of broken
  // proxies. If one alternative proxy is broken, use of all alternative proxies
  // is disabled because it is likely that other QUIC proxies would be
  // broken   too.
  alternative_proxies_broken_ = true;
  UMA_HISTOGRAM_COUNTS_100("DataReductionProxy.Quic.OnAlternativeProxyBroken",
                           1);
}

net::ProxyServer DataReductionProxyDelegate::GetDefaultAlternativeProxy()
    const {
  if (!params::IsZeroRttQuicEnabled())
    return net::ProxyServer();

  if (alternative_proxies_broken_) {
    RecordGetDefaultAlternativeProxy(DEFAULT_ALTERNATIVE_PROXY_STATUS_BROKEN);
    return net::ProxyServer();
  }

  net::ProxyServer proxy_server(
      net::ProxyServer::SCHEME_QUIC,
      net::HostPortPair(kDataReductionCoreProxy, 443));
  if (!config_ || !config_->IsDataReductionProxy(proxy_server, NULL)) {
    RecordGetDefaultAlternativeProxy(
        DEFAULT_ALTERNATIVE_PROXY_STATUS_UNAVAILABLE);
    return net::ProxyServer();
  }

  RecordGetDefaultAlternativeProxy(DEFAULT_ALTERNATIVE_PROXY_STATUS_AVAILABLE);
  return proxy_server;
}

bool DataReductionProxyDelegate::SupportsQUIC(
    const net::ProxyServer& proxy_server) const {
  // Enable QUIC for whitelisted proxies.
  // TODO(tbansal):  Use client config service to control this whitelist.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDataReductionProxyEnableQuicOnNonCoreProxies) ||
         proxy_server ==
             net::ProxyServer(net::ProxyServer::SCHEME_HTTPS,
                              net::HostPortPair(kDataReductionCoreProxy, 443));
}

void DataReductionProxyDelegate::RecordQuicProxyStatus(
    QuicProxyStatus status) const {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.Quic.ProxyStatus", status,
                            QUIC_PROXY_STATUS_BOUNDARY);
}

void DataReductionProxyDelegate::RecordGetDefaultAlternativeProxy(
    DefaultAlternativeProxyStatus status) const {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.Quic.DefaultAlternativeProxy",
                            status, DEFAULT_ALTERNATIVE_PROXY_STATUS_BOUNDARY);
}

void OnResolveProxyHandler(const GURL& url,
                           const std::string& method,
                           const net::ProxyConfig& data_reduction_proxy_config,
                           const net::ProxyRetryInfoMap& proxy_retry_info,
                           const DataReductionProxyConfig* config,
                           net::ProxyInfo* result) {
  DCHECK(config);
  DCHECK(result->is_empty() || result->is_direct() ||
         !config->IsDataReductionProxy(result->proxy_server(), NULL));
  if (!util::EligibleForDataReductionProxy(*result, url, method))
    return;
  net::ProxyInfo data_reduction_proxy_info;
  bool data_saver_proxy_used = util::ApplyProxyConfigToProxyInfo(
      data_reduction_proxy_config, proxy_retry_info, url,
      &data_reduction_proxy_info);
  if (data_saver_proxy_used)
    result->OverrideProxyList(data_reduction_proxy_info.proxy_list());

  // The |data_reduction_proxy_config| must be valid otherwise the proxy
  // cannot be used.
  DCHECK(data_reduction_proxy_config.is_valid() || !data_saver_proxy_used);

  if (config->enabled_by_user_and_reachable() && url.SchemeIsHTTPOrHTTPS() &&
      !url.SchemeIsCryptographic() && !net::IsLocalhost(url.host()) &&
      (!data_reduction_proxy_config.is_valid() || data_saver_proxy_used)) {
    UMA_HISTOGRAM_BOOLEAN("DataReductionProxy.ConfigService.HTTPRequests",
                          data_saver_proxy_used);
  }
}

}  // namespace data_reduction_proxy
