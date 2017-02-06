// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/proxy_resolving_client_socket.h"

#include <stdint.h>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_network_session.h"
#include "net/http/proxy_client_socket.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace jingle_glue {

ProxyResolvingClientSocket::ProxyResolvingClientSocket(
    net::ClientSocketFactory* socket_factory,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const net::SSLConfig& ssl_config,
    const net::HostPortPair& dest_host_port_pair)
        : proxy_resolve_callback_(
              base::Bind(&ProxyResolvingClientSocket::ProcessProxyResolveDone,
                         base::Unretained(this))),
          connect_callback_(
              base::Bind(&ProxyResolvingClientSocket::ProcessConnectDone,
                         base::Unretained(this))),
          ssl_config_(ssl_config),
          pac_request_(NULL),
          dest_host_port_pair_(dest_host_port_pair),
          // Assume that we intend to do TLS on this socket; all
          // current use cases do.
          proxy_url_("https://" + dest_host_port_pair_.ToString()),
          tried_direct_connect_fallback_(false),
          bound_net_log_(
              net::BoundNetLog::Make(
                  request_context_getter->GetURLRequestContext()->net_log(),
                  net::NetLog::SOURCE_SOCKET)),
          weak_factory_(this) {
  DCHECK(request_context_getter.get());
  net::URLRequestContext* request_context =
      request_context_getter->GetURLRequestContext();
  DCHECK(request_context);
  DCHECK(!dest_host_port_pair_.host().empty());
  DCHECK_GT(dest_host_port_pair_.port(), 0);
  DCHECK(proxy_url_.is_valid());

  net::HttpNetworkSession::Params session_params;
  session_params.client_socket_factory = socket_factory;
  session_params.host_resolver = request_context->host_resolver();
  session_params.cert_verifier = request_context->cert_verifier();
  session_params.transport_security_state =
      request_context->transport_security_state();
  session_params.cert_transparency_verifier =
      request_context->cert_transparency_verifier();
  session_params.ct_policy_enforcer = request_context->ct_policy_enforcer();
  // TODO(rkn): This is NULL because ChannelIDService is not thread safe.
  session_params.channel_id_service = NULL;
  session_params.proxy_service = request_context->proxy_service();
  session_params.ssl_config_service = request_context->ssl_config_service();
  session_params.http_auth_handler_factory =
      request_context->http_auth_handler_factory();
  session_params.http_server_properties =
      request_context->http_server_properties();
  session_params.net_log = request_context->net_log();

  const net::HttpNetworkSession::Params* reference_params =
      request_context->GetNetworkSessionParams();
  if (reference_params) {
    // TODO(mmenke):  Just copying specific parameters seems highly regression
    // prone.  Should have a better way to do this.
    session_params.host_mapping_rules = reference_params->host_mapping_rules;
    session_params.ignore_certificate_errors =
        reference_params->ignore_certificate_errors;
    session_params.testing_fixed_http_port =
        reference_params->testing_fixed_http_port;
    session_params.testing_fixed_https_port =
        reference_params->testing_fixed_https_port;
    session_params.enable_spdy31 = reference_params->enable_spdy31;
    session_params.enable_http2 = reference_params->enable_http2;
    session_params.enable_http2_alternative_service_with_different_host =
        reference_params->enable_http2_alternative_service_with_different_host;
    session_params.enable_quic_alternative_service_with_different_host =
        reference_params->enable_quic_alternative_service_with_different_host;
  }

  network_session_.reset(new net::HttpNetworkSession(session_params));
}

ProxyResolvingClientSocket::~ProxyResolvingClientSocket() {
  Disconnect();
}

int ProxyResolvingClientSocket::Read(net::IOBuffer* buf, int buf_len,
                                     const net::CompletionCallback& callback) {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->Read(buf, buf_len, callback);
  NOTREACHED();
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::Write(
    net::IOBuffer* buf,
    int buf_len,
    const net::CompletionCallback& callback) {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->Write(buf, buf_len, callback);
  NOTREACHED();
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::SetReceiveBufferSize(int32_t size) {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->SetReceiveBufferSize(size);
  NOTREACHED();
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::SetSendBufferSize(int32_t size) {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->SetSendBufferSize(size);
  NOTREACHED();
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::Connect(
    const net::CompletionCallback& callback) {
  DCHECK(user_connect_callback_.is_null());

  tried_direct_connect_fallback_ = false;

  // First we try and resolve the proxy.
  int status = network_session_->proxy_service()->ResolveProxy(
      proxy_url_,
      std::string(),
      net::LOAD_NORMAL,
      &proxy_info_,
      proxy_resolve_callback_,
      &pac_request_,
      NULL,
      bound_net_log_);
  if (status != net::ERR_IO_PENDING) {
    // We defer execution of ProcessProxyResolveDone instead of calling it
    // directly here for simplicity. From the caller's point of view,
    // the connect always happens asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ProxyResolvingClientSocket::ProcessProxyResolveDone,
                   weak_factory_.GetWeakPtr(), status));
  }
  user_connect_callback_ = callback;
  return net::ERR_IO_PENDING;
}

void ProxyResolvingClientSocket::RunUserConnectCallback(int status) {
  DCHECK_LE(status, net::OK);
  net::CompletionCallback user_connect_callback = user_connect_callback_;
  user_connect_callback_.Reset();
  user_connect_callback.Run(status);
}

// Always runs asynchronously.
void ProxyResolvingClientSocket::ProcessProxyResolveDone(int status) {
  pac_request_ = NULL;

  DCHECK_NE(status, net::ERR_IO_PENDING);
  if (status == net::OK) {
    // Remove unsupported proxies from the list.
    proxy_info_.RemoveProxiesWithoutScheme(
        net::ProxyServer::SCHEME_DIRECT |
        net::ProxyServer::SCHEME_HTTP | net::ProxyServer::SCHEME_HTTPS |
        net::ProxyServer::SCHEME_SOCKS4 | net::ProxyServer::SCHEME_SOCKS5);

    if (proxy_info_.is_empty()) {
      // No proxies/direct to choose from. This happens when we don't support
      // any of the proxies in the returned list.
      status = net::ERR_NO_SUPPORTED_PROXIES;
    }
  }

  // Since we are faking the URL, it is possible that no proxies match our URL.
  // Try falling back to a direct connection if we have not tried that before.
  if (status != net::OK) {
    if (!tried_direct_connect_fallback_) {
      tried_direct_connect_fallback_ = true;
      proxy_info_.UseDirect();
    } else {
      CloseTransportSocket();
      RunUserConnectCallback(status);
      return;
    }
  }

  transport_.reset(new net::ClientSocketHandle);
  // Now that we have resolved the proxy, we need to connect.
  status = net::InitSocketHandleForRawConnect(
      dest_host_port_pair_, network_session_.get(), proxy_info_, ssl_config_,
      ssl_config_, net::PRIVACY_MODE_DISABLED, bound_net_log_, transport_.get(),
      connect_callback_);
  if (status != net::ERR_IO_PENDING) {
    // Since this method is always called asynchronously. it is OK to call
    // ProcessConnectDone synchronously.
    ProcessConnectDone(status);
  }
}

void ProxyResolvingClientSocket::ProcessConnectDone(int status) {
  if (status != net::OK) {
    // If the connection fails, try another proxy.
    status = ReconsiderProxyAfterError(status);
    // ReconsiderProxyAfterError either returns an error (in which case it is
    // not reconsidering a proxy) or returns ERR_IO_PENDING if it is considering
    // another proxy.
    DCHECK_NE(status, net::OK);
    if (status == net::ERR_IO_PENDING)
      // Proxy reconsideration pending. Return.
      return;
    CloseTransportSocket();
  } else {
    ReportSuccessfulProxyConnection();
  }
  RunUserConnectCallback(status);
}

// TODO(sanjeevr): This has largely been copied from
// HttpStreamFactoryImpl::Job::ReconsiderProxyAfterError. This should be
// refactored into some common place.
// This method reconsiders the proxy on certain errors. If it does reconsider
// a proxy it always returns ERR_IO_PENDING and posts a call to
// ProcessProxyResolveDone with the result of the reconsideration.
int ProxyResolvingClientSocket::ReconsiderProxyAfterError(int error) {
  DCHECK(!pac_request_);
  DCHECK_NE(error, net::OK);
  DCHECK_NE(error, net::ERR_IO_PENDING);
  // A failure to resolve the hostname or any error related to establishing a
  // TCP connection could be grounds for trying a new proxy configuration.
  //
  // Why do this when a hostname cannot be resolved?  Some URLs only make sense
  // to proxy servers.  The hostname in those URLs might fail to resolve if we
  // are still using a non-proxy config.  We need to check if a proxy config
  // now exists that corresponds to a proxy server that could load the URL.
  //
  switch (error) {
    case net::ERR_PROXY_CONNECTION_FAILED:
    case net::ERR_NAME_NOT_RESOLVED:
    case net::ERR_INTERNET_DISCONNECTED:
    case net::ERR_ADDRESS_UNREACHABLE:
    case net::ERR_CONNECTION_CLOSED:
    case net::ERR_CONNECTION_RESET:
    case net::ERR_CONNECTION_REFUSED:
    case net::ERR_CONNECTION_ABORTED:
    case net::ERR_TIMED_OUT:
    case net::ERR_TUNNEL_CONNECTION_FAILED:
    case net::ERR_SOCKS_CONNECTION_FAILED:
      break;
    case net::ERR_SOCKS_CONNECTION_HOST_UNREACHABLE:
      // Remap the SOCKS-specific "host unreachable" error to a more
      // generic error code (this way consumers like the link doctor
      // know to substitute their error page).
      //
      // Note that if the host resolving was done by the SOCSK5 proxy, we can't
      // differentiate between a proxy-side "host not found" versus a proxy-side
      // "address unreachable" error, and will report both of these failures as
      // ERR_ADDRESS_UNREACHABLE.
      return net::ERR_ADDRESS_UNREACHABLE;
    case net::ERR_PROXY_AUTH_REQUESTED: {
      net::ProxyClientSocket* proxy_socket =
          static_cast<net::ProxyClientSocket*>(transport_->socket());

      if (proxy_socket->GetAuthController()->HaveAuth())
        return proxy_socket->RestartWithAuth(connect_callback_);

      return error;
    }
    default:
      return error;
  }

  if (proxy_info_.is_https() && ssl_config_.send_client_cert) {
    network_session_->ssl_client_auth_cache()->Remove(
        proxy_info_.proxy_server().host_port_pair());
  }

  int rv = network_session_->proxy_service()->ReconsiderProxyAfterError(
      proxy_url_, std::string(), net::LOAD_NORMAL, error, &proxy_info_,
      proxy_resolve_callback_, &pac_request_, NULL, bound_net_log_);
  if (rv == net::OK || rv == net::ERR_IO_PENDING) {
    CloseTransportSocket();
  } else {
    // If ReconsiderProxyAfterError() failed synchronously, it means
    // there was nothing left to fall-back to, so fail the transaction
    // with the last connection error we got.
    rv = error;
  }

  // We either have new proxy info or there was an error in falling back.
  // In both cases we want to post ProcessProxyResolveDone (in the error case
  // we might still want to fall back a direct connection).
  if (rv != net::ERR_IO_PENDING) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ProxyResolvingClientSocket::ProcessProxyResolveDone,
                   weak_factory_.GetWeakPtr(), rv));
    // Since we potentially have another try to go (trying the direct connect)
    // set the return code code to ERR_IO_PENDING.
    rv = net::ERR_IO_PENDING;
  }
  return rv;
}

void ProxyResolvingClientSocket::ReportSuccessfulProxyConnection() {
  network_session_->proxy_service()->ReportSuccess(proxy_info_, NULL);
}

void ProxyResolvingClientSocket::Disconnect() {
  CloseTransportSocket();
  if (pac_request_) {
    network_session_->proxy_service()->CancelPacRequest(pac_request_);
    pac_request_ = NULL;
  }
  user_connect_callback_.Reset();
}

bool ProxyResolvingClientSocket::IsConnected() const {
  if (!transport_.get() || !transport_->socket())
    return false;
  return transport_->socket()->IsConnected();
}

bool ProxyResolvingClientSocket::IsConnectedAndIdle() const {
  if (!transport_.get() || !transport_->socket())
    return false;
  return transport_->socket()->IsConnectedAndIdle();
}

int ProxyResolvingClientSocket::GetPeerAddress(
    net::IPEndPoint* address) const {
  if (!transport_.get() || !transport_->socket()) {
    NOTREACHED();
    return net::ERR_SOCKET_NOT_CONNECTED;
  }

  if (proxy_info_.is_direct())
    return transport_->socket()->GetPeerAddress(address);

  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(dest_host_port_pair_.host())) {
    // Do not expose the proxy IP address to the caller.
    return net::ERR_NAME_NOT_RESOLVED;
  }

  *address = net::IPEndPoint(ip_address, dest_host_port_pair_.port());
  return net::OK;
}

int ProxyResolvingClientSocket::GetLocalAddress(
    net::IPEndPoint* address) const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->GetLocalAddress(address);
  NOTREACHED();
  return net::ERR_SOCKET_NOT_CONNECTED;
}

const net::BoundNetLog& ProxyResolvingClientSocket::NetLog() const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->NetLog();
  NOTREACHED();
  return bound_net_log_;
}

void ProxyResolvingClientSocket::SetSubresourceSpeculation() {
  if (transport_.get() && transport_->socket())
    transport_->socket()->SetSubresourceSpeculation();
  else
    NOTREACHED();
}

void ProxyResolvingClientSocket::SetOmniboxSpeculation() {
  if (transport_.get() && transport_->socket())
    transport_->socket()->SetOmniboxSpeculation();
  else
    NOTREACHED();
}

bool ProxyResolvingClientSocket::WasEverUsed() const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->WasEverUsed();
  NOTREACHED();
  return false;
}

bool ProxyResolvingClientSocket::WasNpnNegotiated() const {
  return false;
}

net::NextProto ProxyResolvingClientSocket::GetNegotiatedProtocol() const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->GetNegotiatedProtocol();
  NOTREACHED();
  return net::kProtoUnknown;
}

bool ProxyResolvingClientSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  return false;
}

void ProxyResolvingClientSocket::GetConnectionAttempts(
    net::ConnectionAttempts* out) const {
  out->clear();
}

int64_t ProxyResolvingClientSocket::GetTotalReceivedBytes() const {
  NOTIMPLEMENTED();
  return 0;
}

void ProxyResolvingClientSocket::CloseTransportSocket() {
  if (transport_.get() && transport_->socket())
    transport_->socket()->Disconnect();
  transport_.reset();
}

}  // namespace jingle_glue
