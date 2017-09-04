// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DELEGATE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "net/base/proxy_delegate.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "url/gurl.h"

namespace net {
class HostPortPair;
class HttpRequestHeaders;
class HttpResponseHeaders;
class NetLog;
class ProxyConfig;
class ProxyInfo;
class ProxyService;
class URLRequest;
}

namespace data_reduction_proxy {

class DataReductionProxyBypassStats;
class DataReductionProxyConfig;
class DataReductionProxyConfigurator;
class DataReductionProxyEventCreator;
class DataReductionProxyRequestOptions;

class DataReductionProxyDelegate : public net::ProxyDelegate {
 public:
  // ProxyDelegate instance is owned by io_thread. |auth_handler| and |config|
  // outlives this class instance.
  explicit DataReductionProxyDelegate(
      DataReductionProxyConfig* config,
      const DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventCreator* event_creator,
      DataReductionProxyBypassStats* bypass_stats,
      net::NetLog* net_log);

  ~DataReductionProxyDelegate() override;

  // net::ProxyDelegate implementation:
  void OnResolveProxy(const GURL& url,
                      const std::string& method,
                      const net::ProxyService& proxy_service,
                      net::ProxyInfo* result) override;
  void OnFallback(const net::ProxyServer& bad_proxy, int net_error) override;
  void OnBeforeTunnelRequest(const net::HostPortPair& proxy_server,
                             net::HttpRequestHeaders* extra_headers) override;
  void OnTunnelConnectCompleted(const net::HostPortPair& endpoint,
                                const net::HostPortPair& proxy_server,
                                int net_error) override;
  bool IsTrustedSpdyProxy(const net::ProxyServer& proxy_server) override;
  void OnTunnelHeadersReceived(
      const net::HostPortPair& origin,
      const net::HostPortPair& proxy_server,
      const net::HttpResponseHeaders& response_headers) override;

 protected:
  // Protected so that these methods are accessible for testing.
  // net::ProxyDelegate implementation:
  void GetAlternativeProxy(
      const GURL& url,
      const net::ProxyServer& resolved_proxy_server,
      net::ProxyServer* alternative_proxy_server) const override;
  void OnAlternativeProxyBroken(
      const net::ProxyServer& alternative_proxy_server) override;
  net::ProxyServer GetDefaultAlternativeProxy() const override;

  // Protected so that it can be overridden during testing.
  // Returns true if |proxy_server| supports QUIC.
  virtual bool SupportsQUIC(const net::ProxyServer& proxy_server) const;

  // Availability status of data reduction QUIC proxy.
  // Protected so that the enum values are accessible for testing.
  enum QuicProxyStatus {
    QUIC_PROXY_STATUS_AVAILABLE,
    QUIC_PROXY_NOT_SUPPORTED,
    QUIC_PROXY_STATUS_MARKED_AS_BROKEN,
    QUIC_PROXY_STATUS_BOUNDARY
  };

  // Availability status of data reduction proxy that supports 0-RTT QUIC.
  // Protected so that the enum values are accessible for testing.
  enum DefaultAlternativeProxyStatus {
    DEFAULT_ALTERNATIVE_PROXY_STATUS_AVAILABLE,
    DEFAULT_ALTERNATIVE_PROXY_STATUS_BROKEN,
    DEFAULT_ALTERNATIVE_PROXY_STATUS_UNAVAILABLE,
    DEFAULT_ALTERNATIVE_PROXY_STATUS_BOUNDARY,
  };

 private:
  // Records the availability status of data reduction proxy.
  void RecordQuicProxyStatus(QuicProxyStatus status) const;

  // Records the availability status of data reduction proxy that supports 0-RTT
  // QUIC.
  void RecordGetDefaultAlternativeProxy(
      DefaultAlternativeProxyStatus status) const;

  const DataReductionProxyConfig* config_;
  const DataReductionProxyConfigurator* configurator_;
  DataReductionProxyEventCreator* event_creator_;
  DataReductionProxyBypassStats* bypass_stats_;

  // True if the use of alternate proxies is disabled.
  bool alternative_proxies_broken_;

  net::NetLog* net_log_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyDelegate);
};

// Adds data reduction proxies to |result|, where applicable, if result
// otherwise uses a direct connection for |url|, and the data reduction proxy is
// not bypassed. Also, configures |result| to proceed directly to the origin if
// |result|'s current proxy is the data reduction proxy
// This is visible for test purposes.
void OnResolveProxyHandler(const GURL& url,
                           const std::string& method,
                           const net::ProxyConfig& data_reduction_proxy_config,
                           const net::ProxyRetryInfoMap& proxy_retry_info,
                           const DataReductionProxyConfig* config,
                           net::ProxyInfo* result);
}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DELEGATE_H_
