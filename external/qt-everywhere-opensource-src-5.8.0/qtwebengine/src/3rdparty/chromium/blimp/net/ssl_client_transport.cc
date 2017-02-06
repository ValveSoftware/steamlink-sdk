// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/ssl_client_transport.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "blimp/net/exact_match_cert_verifier.h"
#include "blimp/net/stream_socket_connection.h"
#include "net/base/host_port_pair.h"
#include "net/cert/x509_certificate.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/ssl_config.h"

namespace blimp {

SSLClientTransport::SSLClientTransport(const net::IPEndPoint& ip_endpoint,
                                       scoped_refptr<net::X509Certificate> cert,
                                       BlimpConnectionStatistics* statistics,
                                       net::NetLog* net_log)
    : TCPClientTransport(ip_endpoint, statistics, net_log),
      ip_endpoint_(ip_endpoint) {
  // Test code may pass in a null value for |cert|. Only spin up a CertVerifier
  // if there is a cert present.
  if (cert) {
    cert_verifier_.reset(new ExactMatchCertVerifier(std::move(cert)));
  }
}

SSLClientTransport::~SSLClientTransport() {}

const char* SSLClientTransport::GetName() const {
  return "SSL";
}

void SSLClientTransport::OnTCPConnectComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);

  std::unique_ptr<net::StreamSocket> tcp_socket =
      TCPClientTransport::TakeSocket();

  DVLOG(1) << "TCP connection result=" << result;
  if (result != net::OK) {
    OnConnectComplete(result);
    return;
  }

  // Construct arguments to use for the SSL socket factory.
  std::unique_ptr<net::ClientSocketHandle> socket_handle(
      new net::ClientSocketHandle);
  socket_handle->SetSocket(std::move(tcp_socket));

  net::HostPortPair host_port_pair =
      net::HostPortPair::FromIPEndPoint(ip_endpoint_);

  net::SSLClientSocketContext create_context;
  create_context.cert_verifier = cert_verifier_.get();
  create_context.transport_security_state = &transport_security_state_;
  create_context.ct_policy_enforcer = &ct_policy_enforcer_;
  create_context.cert_transparency_verifier = &cert_transparency_verifier_;

  std::unique_ptr<net::StreamSocket> ssl_socket(
      socket_factory()->CreateSSLClientSocket(std::move(socket_handle),
                                              host_port_pair, net::SSLConfig(),
                                              create_context));

  if (!ssl_socket) {
    OnConnectComplete(net::ERR_SSL_PROTOCOL_ERROR);
    return;
  }

  result = ssl_socket->Connect(base::Bind(
      &SSLClientTransport::OnSSLConnectComplete, base::Unretained(this)));
  SetSocket(std::move(ssl_socket));

  if (result == net::ERR_IO_PENDING) {
    // SSL connection will complete asynchronously.
    return;
  }

  OnSSLConnectComplete(result);
}

void SSLClientTransport::OnSSLConnectComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  DVLOG(1) << "SSL connection result=" << result;

  OnConnectComplete(result);
}

}  // namespace blimp
