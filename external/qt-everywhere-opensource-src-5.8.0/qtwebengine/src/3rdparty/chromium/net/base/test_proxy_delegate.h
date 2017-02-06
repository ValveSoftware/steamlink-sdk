// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TEST_PROXY_DELEGATE_H_
#define NET_BASE_TEST_PROXY_DELEGATE_H_

#include <string>

#include "net/base/host_port_pair.h"
#include "net/base/proxy_delegate.h"
#include "net/proxy/proxy_server.h"

class GURL;

namespace net {

class HttpRequestHeaders;
class HttpResponseHeaders;
class ProxyInfo;
class ProxyService;
class URLRequest;

class TestProxyDelegate : public ProxyDelegate {
 public:
  TestProxyDelegate();
  ~TestProxyDelegate() override;

  bool on_before_tunnel_request_called() const {
    return on_before_tunnel_request_called_;
  }

  bool on_tunnel_request_completed_called() const {
    return on_tunnel_request_completed_called_;
  }

  bool on_tunnel_headers_received_called() const {
    return on_tunnel_headers_received_called_;
  }

  void set_trusted_spdy_proxy(const net::ProxyServer& proxy_server) {
    trusted_spdy_proxy_ = proxy_server;
  }

  void VerifyOnTunnelRequestCompleted(const std::string& endpoint,
                                      const std::string& proxy_server) const;

  void VerifyOnTunnelHeadersReceived(const std::string& origin,
                                     const std::string& proxy_server,
                                     const std::string& status_line) const;

  // ProxyDelegate implementation:
  void OnResolveProxy(const GURL& url,
                      const std::string& method,
                      int load_flags,
                      const ProxyService& proxy_service,
                      ProxyInfo* result) override;
  void OnTunnelConnectCompleted(const HostPortPair& endpoint,
                                const HostPortPair& proxy_server,
                                int net_error) override;
  void OnFallback(const ProxyServer& bad_proxy, int net_error) override;
  void OnBeforeTunnelRequest(const HostPortPair& proxy_server,
                             HttpRequestHeaders* extra_headers) override;
  void OnTunnelHeadersReceived(
      const HostPortPair& origin,
      const HostPortPair& proxy_server,
      const HttpResponseHeaders& response_headers) override;
  bool IsTrustedSpdyProxy(const net::ProxyServer& proxy_server) override;

 private:
  bool on_before_tunnel_request_called_;
  bool on_tunnel_request_completed_called_;
  bool on_tunnel_headers_received_called_;
  net::ProxyServer trusted_spdy_proxy_;
  HostPortPair on_tunnel_request_completed_endpoint_;
  HostPortPair on_tunnel_request_completed_proxy_server_;
  HostPortPair on_tunnel_headers_received_origin_;
  HostPortPair on_tunnel_headers_received_proxy_server_;
  std::string on_tunnel_headers_received_status_line_;
};

}  // namespace net

#endif  // NET_BASE_TEST_PROXY_DELEGATE_H_