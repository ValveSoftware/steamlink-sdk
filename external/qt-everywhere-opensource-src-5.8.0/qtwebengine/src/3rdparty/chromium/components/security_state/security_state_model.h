// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_SECURITY_STATE_MODEL_H_
#define COMPONENTS_SECURITY_STATE_SECURITY_STATE_MODEL_H_

#include "base/macros.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

namespace security_state {

class SecurityStateModelClient;

// SecurityStateModel provides high-level security information about a
// page or request.
//
// SecurityStateModel::SecurityInfo is the main data structure computed
// by a SecurityStateModel. SecurityInfo contains a SecurityLevel (which
// is a single value describing the overall security state) along with
// information that a consumer might want to display in UI to explain or
// elaborate on the SecurityLevel.
class SecurityStateModel {
 public:
  // Describes the overall security state of the page.
  //
  // If you reorder, add, or delete values from this enum, you must also
  // update the UI icons in ToolbarModelImpl::GetIconForSecurityLevel.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.security_state
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: ConnectionSecurityLevel
  enum SecurityLevel {
    // HTTP/no URL/HTTPS but with insecure passive content on the page
    NONE,

    // HTTPS with valid EV cert
    EV_SECURE,

    // HTTPS (non-EV) with valid cert
    SECURE,

    // HTTPS, but with an outdated protocol version
    SECURITY_WARNING,

    // HTTPS, but the certificate verification chain is anchored on a
    // certificate that was installed by the system administrator
    SECURITY_POLICY_WARNING,

    // Attempted HTTPS and failed, page not authenticated, or HTTPS with
    // insecure active content on the page
    SECURITY_ERROR,
  };

  // Describes how the SHA1 deprecation policy applies to an HTTPS
  // connection.
  enum SHA1DeprecationStatus {
    // No SHA1 deprecation policy applies.
    NO_DEPRECATED_SHA1,
    // The connection used a certificate with a SHA1 signature in the
    // chain, and policy says that the connection should be treated with a
    // warning.
    DEPRECATED_SHA1_MINOR,
    // The connection used a certificate with a SHA1 signature in the
    // chain, and policy says that the connection should be treated as
    // broken HTTPS.
    DEPRECATED_SHA1_MAJOR,
  };

  // Describes the type of mixed content (if any) that a site
  // displayed/ran.
  enum MixedContentStatus {
    NO_MIXED_CONTENT,
    // The site displayed insecure resources (passive mixed content).
    DISPLAYED_MIXED_CONTENT,
    // The site ran insecure code (active mixed content).
    RAN_MIXED_CONTENT,
    // The site both ran and displayed insecure resources.
    RAN_AND_DISPLAYED_MIXED_CONTENT,
  };

  // Describes the security status of a page or request. This is the
  // main data structure provided by this class.
  struct SecurityInfo {
    SecurityInfo();
    ~SecurityInfo();
    SecurityLevel security_level;
    SHA1DeprecationStatus sha1_deprecation_status;
    MixedContentStatus mixed_content_status;
    // The verification statuses of the signed certificate timestamps
    // for the connection.
    std::vector<net::ct::SCTVerifyStatus> sct_verify_statuses;
    bool scheme_is_cryptographic;
    net::CertStatus cert_status;
    int cert_id;
    // The security strength, in bits, of the SSL cipher suite. In late
    // 2015, 128 is considered the minimum.
    // 0 means the connection is not encrypted.
    // -1 means the security strength is unknown.
    int security_bits;
    // Information about the SSL connection, such as protocol and
    // ciphersuite. See ssl_connection_flags.h in net.
    int connection_status;
    // True if the protocol version and ciphersuite for the connection
    // are considered secure.
    bool is_secure_protocol_and_ciphersuite;

    // True if pinning was bypassed due to a local trust anchor.
    bool pkp_bypassed;
  };

  // Contains the security state relevant to computing the SecurityInfo
  // for a page. This is the input to GetSecurityInfo() provided by the
  // model's client.
  struct VisibleSecurityState {
    VisibleSecurityState();
    ~VisibleSecurityState();
    bool operator==(const VisibleSecurityState& other) const;
    bool initialized;
    GURL url;
    // The baseline SecurityLevel describing the page or request before
    // any SecurityStateModel policies have been applied.
    SecurityLevel initial_security_level;
    // The following fields contain information about the connection
    // used to load the page or request.
    int cert_id;
    net::CertStatus cert_status;
    int connection_status;
    int security_bits;
    // The verification statuses of the Signed Certificate
    // Timestamps (if any) that the server provided.
    std::vector<net::ct::SCTVerifyStatus> sct_verify_statuses;
    // True if the page displayed passive mixed content.
    bool displayed_mixed_content;
    // True if the page ran active mixed content.
    bool ran_mixed_content;
    // True if PKP was bypassed due to a local trust anchor.
    bool pkp_bypassed;
  };

  // These security levels describe the treatment given to pages that
  // display and run mixed content. They are used to coordinate the
  // treatment of mixed content with other security UI elements.
  static const SecurityLevel kDisplayedInsecureContentLevel;
  static const SecurityLevel kRanInsecureContentLevel;

  SecurityStateModel();
  virtual ~SecurityStateModel();

  // Returns a SecurityInfo describing the current page. Results are
  // cached so that computation is only done when the relevant security
  // state has changed.
  const SecurityInfo& GetSecurityInfo() const;

  void SetClient(SecurityStateModelClient* client);

 private:
  // Caches the SecurityInfo for the visible page. Marked
  // mutable so that the const accessor GetSecurityInfo() can update the
  // cached values.
  mutable SecurityInfo security_info_;
  mutable VisibleSecurityState visible_security_state_;

  SecurityStateModelClient* client_;

  DISALLOW_COPY_AND_ASSIGN(SecurityStateModel);
};

}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_SECURITY_STATE_MODEL_H_
