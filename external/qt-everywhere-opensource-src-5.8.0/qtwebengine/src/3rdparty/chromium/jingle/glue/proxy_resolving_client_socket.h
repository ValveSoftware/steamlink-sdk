// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This StreamSocket implementation wraps a ClientSocketHandle that is created
// from the client socket pool after resolving proxies.

#ifndef JINGLE_GLUE_PROXY_RESOLVING_CLIENT_SOCKET_H_
#define JINGLE_GLUE_PROXY_RESOLVING_CLIENT_SOCKET_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_config_service.h"
#include "url/gurl.h"

namespace net {
class ClientSocketFactory;
class ClientSocketHandle;
class HttpNetworkSession;
class URLRequestContextGetter;
}  // namespace net

// TODO(sanjeevr): Move this to net/
namespace jingle_glue {

class ProxyResolvingClientSocket : public net::StreamSocket {
 public:
  // Constructs a new ProxyResolvingClientSocket. |socket_factory| is
  // the ClientSocketFactory that will be used by the underlying
  // HttpNetworkSession.  If |socket_factory| is NULL, the default
  // socket factory (net::ClientSocketFactory::GetDefaultFactory())
  // will be used.  |dest_host_port_pair| is the destination for this
  // socket.  The hostname must be non-empty and the port must be > 0.
  ProxyResolvingClientSocket(
      net::ClientSocketFactory* socket_factory,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const net::SSLConfig& ssl_config,
      const net::HostPortPair& dest_host_port_pair);
  ~ProxyResolvingClientSocket() override;

  // net::StreamSocket implementation.
  int Read(net::IOBuffer* buf,
           int buf_len,
           const net::CompletionCallback& callback) override;
  int Write(net::IOBuffer* buf,
            int buf_len,
            const net::CompletionCallback& callback) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int Connect(const net::CompletionCallback& callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(net::IPEndPoint* address) const override;
  int GetLocalAddress(net::IPEndPoint* address) const override;
  const net::BoundNetLog& NetLog() const override;
  void SetSubresourceSpeculation() override;
  void SetOmniboxSpeculation() override;
  bool WasEverUsed() const override;
  bool WasNpnNegotiated() const override;
  net::NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(net::SSLInfo* ssl_info) override;
  void GetConnectionAttempts(net::ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const net::ConnectionAttempts& attempts) override {
  }
  int64_t GetTotalReceivedBytes() const override;

 private:
  // Proxy resolution and connection functions.
  void ProcessProxyResolveDone(int status);
  void ProcessConnectDone(int status);

  void CloseTransportSocket();
  void RunUserConnectCallback(int status);
  int ReconsiderProxyAfterError(int error);
  void ReportSuccessfulProxyConnection();

  // Callbacks passed to net APIs.
  net::CompletionCallback proxy_resolve_callback_;
  net::CompletionCallback connect_callback_;

  std::unique_ptr<net::HttpNetworkSession> network_session_;

  // The transport socket.
  std::unique_ptr<net::ClientSocketHandle> transport_;

  const net::SSLConfig ssl_config_;
  net::ProxyService::PacRequest* pac_request_;
  net::ProxyInfo proxy_info_;
  const net::HostPortPair dest_host_port_pair_;
  const GURL proxy_url_;
  bool tried_direct_connect_fallback_;
  net::BoundNetLog bound_net_log_;

  // The callback passed to Connect().
  net::CompletionCallback user_connect_callback_;

  base::WeakPtrFactory<ProxyResolvingClientSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolvingClientSocket);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_PROXY_RESOLVING_CLIENT_SOCKET_H_
