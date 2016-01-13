// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "net/base/completion_callback.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/socket/ssl_socket.h"
#include "net/socket/stream_socket.h"

namespace net {

class CertVerifier;
class CTVerifier;
class ServerBoundCertService;
class SSLCertRequestInfo;
struct SSLConfig;
class SSLInfo;
class TransportSecurityState;
class X509Certificate;

// This struct groups together several fields which are used by various
// classes related to SSLClientSocket.
struct SSLClientSocketContext {
  SSLClientSocketContext()
      : cert_verifier(NULL),
        server_bound_cert_service(NULL),
        transport_security_state(NULL),
        cert_transparency_verifier(NULL) {}

  SSLClientSocketContext(CertVerifier* cert_verifier_arg,
                         ServerBoundCertService* server_bound_cert_service_arg,
                         TransportSecurityState* transport_security_state_arg,
                         CTVerifier* cert_transparency_verifier_arg,
                         const std::string& ssl_session_cache_shard_arg)
      : cert_verifier(cert_verifier_arg),
        server_bound_cert_service(server_bound_cert_service_arg),
        transport_security_state(transport_security_state_arg),
        cert_transparency_verifier(cert_transparency_verifier_arg),
        ssl_session_cache_shard(ssl_session_cache_shard_arg) {}

  CertVerifier* cert_verifier;
  ServerBoundCertService* server_bound_cert_service;
  TransportSecurityState* transport_security_state;
  CTVerifier* cert_transparency_verifier;
  // ssl_session_cache_shard is an opaque string that identifies a shard of the
  // SSL session cache. SSL sockets with the same ssl_session_cache_shard may
  // resume each other's SSL sessions but we'll never sessions between shards.
  const std::string ssl_session_cache_shard;
};

// A client socket that uses SSL as the transport layer.
//
// NOTE: The SSL handshake occurs within the Connect method after a TCP
// connection is established.  If a SSL error occurs during the handshake,
// Connect will fail.
//
class NET_EXPORT SSLClientSocket : public SSLSocket {
 public:
  SSLClientSocket();

  // Next Protocol Negotiation (NPN) allows a TLS client and server to come to
  // an agreement about the application level protocol to speak over a
  // connection.
  enum NextProtoStatus {
    // WARNING: These values are serialized to disk. Don't change them.

    kNextProtoUnsupported = 0,  // The server doesn't support NPN.
    kNextProtoNegotiated = 1,   // We agreed on a protocol.
    kNextProtoNoOverlap = 2,    // No protocols in common. We requested
                                // the first protocol in our list.
  };

  // StreamSocket:
  virtual bool WasNpnNegotiated() const OVERRIDE;
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE;

  // Gets the SSL CertificateRequest info of the socket after Connect failed
  // with ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) = 0;

  // Get the application level protocol that we negotiated with the server.
  // *proto is set to the resulting protocol (n.b. that the string may have
  // embedded NULs).
  //   kNextProtoUnsupported: *proto is cleared.
  //   kNextProtoNegotiated:  *proto is set to the negotiated protocol.
  //   kNextProtoNoOverlap:   *proto is set to the first protocol in the
  //                          supported list.
  // *server_protos is set to the server advertised protocols.
  virtual NextProtoStatus GetNextProto(std::string* proto,
                                       std::string* server_protos) = 0;

  static NextProto NextProtoFromString(const std::string& proto_string);

  static const char* NextProtoToString(NextProto next_proto);

  static const char* NextProtoStatusToString(const NextProtoStatus status);

  // Can be used with the second argument(|server_protos|) of |GetNextProto| to
  // construct a comma separated string of server advertised protocols.
  static std::string ServerProtosToString(const std::string& server_protos);

  static bool IgnoreCertError(int error, int load_flags);

  // ClearSessionCache clears the SSL session cache, used to resume SSL
  // sessions.
  static void ClearSessionCache();

  virtual bool set_was_npn_negotiated(bool negotiated);

  virtual bool was_spdy_negotiated() const;

  virtual bool set_was_spdy_negotiated(bool negotiated);

  virtual void set_protocol_negotiated(NextProto protocol_negotiated);

  // Returns the ServerBoundCertService used by this socket, or NULL if
  // server bound certificates are not supported.
  virtual ServerBoundCertService* GetServerBoundCertService() const = 0;

  // Returns true if a channel ID was sent on this connection.
  // This may be useful for protocols, like SPDY, which allow the same
  // connection to be shared between multiple domains, each of which need
  // a channel ID.
  //
  // Public for ssl_client_socket_openssl_unittest.cc.
  virtual bool WasChannelIDSent() const;

 protected:
  virtual void set_channel_id_sent(bool channel_id_sent);

  virtual void set_signed_cert_timestamps_received(
      bool signed_cert_timestamps_received);

  virtual void set_stapled_ocsp_response_received(
      bool stapled_ocsp_response_received);

  // Records histograms for channel id support during full handshakes - resumed
  // handshakes are ignored.
  static void RecordChannelIDSupport(
      ServerBoundCertService* server_bound_cert_service,
      bool negotiated_channel_id,
      bool channel_id_enabled,
      bool supports_ecc);

  // Returns whether TLS channel ID is enabled.
  static bool IsChannelIDEnabled(
      const SSLConfig& ssl_config,
      ServerBoundCertService* server_bound_cert_service);

  // For unit testing only.
  // Returns the unverified certificate chain as presented by server.
  // Note that chain may be different than the verified chain returned by
  // StreamSocket::GetSSLInfo().
  virtual scoped_refptr<X509Certificate> GetUnverifiedServerCertificateChain()
      const = 0;

 private:
  // For signed_cert_timestamps_received_ and stapled_ocsp_response_received_.
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocketTest,
                           ConnectSignedCertTimestampsEnabledTLSExtension);
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocketTest,
                           ConnectSignedCertTimestampsEnabledOCSP);
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocketTest,
                           ConnectSignedCertTimestampsDisabled);
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocketTest,
                           VerifyServerChainProperlyOrdered);

  // True if NPN was responded to, independent of selecting SPDY or HTTP.
  bool was_npn_negotiated_;
  // True if NPN successfully negotiated SPDY.
  bool was_spdy_negotiated_;
  // Protocol that we negotiated with the server.
  NextProto protocol_negotiated_;
  // True if a channel ID was sent.
  bool channel_id_sent_;
  // True if SCTs were received via a TLS extension.
  bool signed_cert_timestamps_received_;
  // True if a stapled OCSP response was received.
  bool stapled_ocsp_response_received_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_H_
