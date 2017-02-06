// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_SSL_CLIENT_TRANSPORT_H_
#define BLIMP_NET_SSL_CLIENT_TRANSPORT_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/blimp_transport.h"
#include "blimp/net/exact_match_cert_verifier.h"
#include "blimp/net/tcp_client_transport.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/http/transport_security_state.h"

namespace net {
class ClientSocketFactory;
class NetLog;
class StreamSocket;
class TCPClientSocket;
class TransportSecurityState;
}  // namespace net

namespace blimp {

class BlimpConnection;
class BlimpConnectionStatistics;

// Creates and connects SSL socket connections to an Engine.
class BLIMP_NET_EXPORT SSLClientTransport : public TCPClientTransport {
 public:
  // |ip_endpoint|: the address to connect to.
  // |cert|: the certificate required from the remote peer.
  //     SSL connections that use different certificates are rejected.
  // |net_log|: the socket event log (optional).
  SSLClientTransport(const net::IPEndPoint& ip_endpoint,
                     scoped_refptr<net::X509Certificate> cert,
                     BlimpConnectionStatistics* statistics,
                     net::NetLog* net_log);

  ~SSLClientTransport() override;

  // BlimpTransport implementation.
  const char* GetName() const override;

 private:
  // Method called after TCPClientSocket::Connect finishes.
  void OnTCPConnectComplete(int result) override;

  // Method called after SSLClientSocket::Connect finishes.
  void OnSSLConnectComplete(int result);

  net::IPEndPoint ip_endpoint_;
  std::unique_ptr<ExactMatchCertVerifier> cert_verifier_;
  net::TransportSecurityState transport_security_state_;
  net::MultiLogCTVerifier cert_transparency_verifier_;
  net::CTPolicyEnforcer ct_policy_enforcer_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientTransport);
};

}  // namespace blimp

#endif  // BLIMP_NET_SSL_CLIENT_TRANSPORT_H_
