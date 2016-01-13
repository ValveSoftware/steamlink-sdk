// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/xmpp_client_socket_factory.h"

#include "base/logging.h"
#include "jingle/glue/fake_ssl_client_socket.h"
#include "jingle/glue/proxy_resolving_client_socket.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace jingle_glue {

XmppClientSocketFactory::XmppClientSocketFactory(
    net::ClientSocketFactory* client_socket_factory,
    const net::SSLConfig& ssl_config,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    bool use_fake_ssl_client_socket)
    : client_socket_factory_(client_socket_factory),
      request_context_getter_(request_context_getter),
      ssl_config_(ssl_config),
      use_fake_ssl_client_socket_(use_fake_ssl_client_socket) {
  CHECK(client_socket_factory_);
}

XmppClientSocketFactory::~XmppClientSocketFactory() {}

scoped_ptr<net::StreamSocket>
XmppClientSocketFactory::CreateTransportClientSocket(
    const net::HostPortPair& host_and_port) {
  // TODO(akalin): Use socket pools.
  scoped_ptr<net::StreamSocket> transport_socket(
      new ProxyResolvingClientSocket(
          NULL,
          request_context_getter_,
          ssl_config_,
          host_and_port));
  return (use_fake_ssl_client_socket_ ?
          scoped_ptr<net::StreamSocket>(
              new FakeSSLClientSocket(transport_socket.Pass())) :
          transport_socket.Pass());
}

scoped_ptr<net::SSLClientSocket>
XmppClientSocketFactory::CreateSSLClientSocket(
    scoped_ptr<net::ClientSocketHandle> transport_socket,
    const net::HostPortPair& host_and_port) {
  net::SSLClientSocketContext context;
  context.cert_verifier =
      request_context_getter_->GetURLRequestContext()->cert_verifier();
  context.transport_security_state = request_context_getter_->
      GetURLRequestContext()->transport_security_state();
  DCHECK(context.transport_security_state);
  // TODO(rkn): context.server_bound_cert_service is NULL because the
  // ServerBoundCertService class is not thread safe.
  return client_socket_factory_->CreateSSLClientSocket(
      transport_socket.Pass(), host_and_port, ssl_config_, context);
}


}  // namespace jingle_glue
