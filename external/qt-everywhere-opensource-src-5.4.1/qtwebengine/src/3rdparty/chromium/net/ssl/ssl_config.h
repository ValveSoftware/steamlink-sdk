// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CONFIG_H_
#define NET_SSL_SSL_CONFIG_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

namespace net {

// Various TLS/SSL ProtocolVersion values encoded as uint16
//      struct {
//          uint8 major;
//          uint8 minor;
//      } ProtocolVersion;
// The most significant byte is |major|, and the least significant byte
// is |minor|.
enum {
  SSL_PROTOCOL_VERSION_SSL3 = 0x0300,
  SSL_PROTOCOL_VERSION_TLS1 = 0x0301,
  SSL_PROTOCOL_VERSION_TLS1_1 = 0x0302,
  SSL_PROTOCOL_VERSION_TLS1_2 = 0x0303,
};

// Default minimum protocol version.
NET_EXPORT extern const uint16 kDefaultSSLVersionMin;

// Default maximum protocol version.
NET_EXPORT extern const uint16 kDefaultSSLVersionMax;

// A collection of SSL-related configuration settings.
struct NET_EXPORT SSLConfig {
  // Default to revocation checking.
  // Default to SSL 3.0 ~ default_version_max() on.
  SSLConfig();
  ~SSLConfig();

  // Returns true if |cert| is one of the certs in |allowed_bad_certs|.
  // The expected cert status is written to |cert_status|. |*cert_status| can
  // be NULL if user doesn't care about the cert status.
  bool IsAllowedBadCert(X509Certificate* cert, CertStatus* cert_status) const;

  // Same as above except works with DER encoded certificates instead
  // of X509Certificate.
  bool IsAllowedBadCert(const base::StringPiece& der_cert,
                        CertStatus* cert_status) const;

  // rev_checking_enabled is true if online certificate revocation checking is
  // enabled (i.e. OCSP and CRL fetching).
  //
  // Regardless of this flag, CRLSet checking is always enabled and locally
  // cached revocation information will be considered.
  bool rev_checking_enabled;

  // rev_checking_required_local_anchors is true if revocation checking is
  // required to succeed when certificates chain to local trust anchors (that
  // is, non-public CAs). If revocation information cannot be obtained, such
  // certificates will be treated as revoked ("hard-fail").
  // Note: This is distinct from rev_checking_enabled. If true, it is
  // equivalent to also setting rev_checking_enabled, but only when the
  // certificate chain chains to a local (non-public) trust anchor.
  bool rev_checking_required_local_anchors;

  // The minimum and maximum protocol versions that are enabled.
  // SSL 3.0 is 0x0300, TLS 1.0 is 0x0301, TLS 1.1 is 0x0302, and so on.
  // (Use the SSL_PROTOCOL_VERSION_xxx enumerators defined above.)
  // SSL 2.0 is not supported. If version_max < version_min, it means no
  // protocol versions are enabled.
  uint16 version_min;
  uint16 version_max;

  // Presorted list of cipher suites which should be explicitly prevented from
  // being used in addition to those disabled by the net built-in policy.
  //
  // By default, all cipher suites supported by the underlying SSL
  // implementation will be enabled except for:
  // - Null encryption cipher suites.
  // - Weak cipher suites: < 80 bits of security strength.
  // - FORTEZZA cipher suites (obsolete).
  // - IDEA cipher suites (RFC 5469 explains why).
  // - Anonymous cipher suites.
  // - ECDSA cipher suites on platforms that do not support ECDSA signed
  //   certificates, as servers may use the presence of such ciphersuites as a
  //   hint to send an ECDSA certificate.
  // The ciphers listed in |disabled_cipher_suites| will be removed in addition
  // to the above list.
  //
  // Though cipher suites are sent in TLS as "uint8 CipherSuite[2]", in
  // big-endian form, they should be declared in host byte order, with the
  // first uint8 occupying the most significant byte.
  // Ex: To disable TLS_RSA_WITH_RC4_128_MD5, specify 0x0004, while to
  // disable TLS_ECDH_ECDSA_WITH_RC4_128_SHA, specify 0xC002.
  std::vector<uint16> disabled_cipher_suites;

  bool channel_id_enabled;   // True if TLS channel ID extension is enabled.
  bool false_start_enabled;  // True if we'll use TLS False Start.
  // True if the Certificate Transparency signed_certificate_timestamp
  // TLS extension is enabled.
  bool signed_cert_timestamps_enabled;

  // require_forward_secrecy, if true, causes only (EC)DHE cipher suites to be
  // enabled. NOTE: this only applies to server sockets currently, although
  // that could be extended if needed.
  bool require_forward_secrecy;

  // TODO(wtc): move the following members to a new SSLParams structure.  They
  // are not SSL configuration settings.

  struct NET_EXPORT CertAndStatus {
    CertAndStatus();
    ~CertAndStatus();

    std::string der_cert;
    CertStatus cert_status;
  };

  // Add any known-bad SSL certificate (with its cert status) to
  // |allowed_bad_certs| that should not trigger an ERR_CERT_* error when
  // calling SSLClientSocket::Connect.  This would normally be done in
  // response to the user explicitly accepting the bad certificate.
  std::vector<CertAndStatus> allowed_bad_certs;

  // True if we should send client_cert to the server.
  bool send_client_cert;

  bool verify_ev_cert;  // True if we should verify the certificate for EV.

  bool version_fallback;  // True if we are falling back to an older protocol
                          // version (one still needs to decrement
                          // version_max).

  // If cert_io_enabled is false, then certificate verification will not
  // result in additional HTTP requests. (For example: to fetch missing
  // intermediates or to perform OCSP/CRL fetches.) It also implies that online
  // revocation checking is disabled.
  // NOTE: Only used by NSS.
  bool cert_io_enabled;

  // The list of application level protocols supported. If set, this will
  // enable Next Protocol Negotiation (if supported). The order of the
  // protocols doesn't matter expect for one case: if the server supports Next
  // Protocol Negotiation, but there is no overlap between the server's and
  // client's protocol sets, then the first protocol in this list will be
  // requested by the client.
  std::vector<std::string> next_protos;

  scoped_refptr<X509Certificate> client_cert;
};

}  // namespace net

#endif  // NET_SSL_SSL_CONFIG_H_
