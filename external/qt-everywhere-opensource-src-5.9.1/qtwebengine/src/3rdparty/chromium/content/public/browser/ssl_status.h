// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SSL_STATUS_H_
#define CONTENT_PUBLIC_BROWSER_SSL_STATUS_H_

#include <stdint.h>

#include <vector>

#include "content/common/content_export.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/x509_certificate.h"

namespace net {
class SSLInfo;
}

namespace content {

// Collects the SSL information for this NavigationEntry.
struct CONTENT_EXPORT SSLStatus {
  // Flags used for the page security content status.
  enum ContentStatusFlags {
    // HTTP page, or HTTPS page with no insecure content.
    NORMAL_CONTENT = 0,

    // HTTPS page containing "displayed" HTTP resources (e.g. images, CSS).
    DISPLAYED_INSECURE_CONTENT = 1 << 0,

    // HTTPS page containing "executed" HTTP resources (i.e. script).
    RAN_INSECURE_CONTENT = 1 << 1,

    // HTTPS page containing "displayed" HTTPS resources (e.g. images,
    // CSS) loaded with certificate errors.
    DISPLAYED_CONTENT_WITH_CERT_ERRORS = 1 << 2,

    // HTTPS page containing "executed" HTTPS resources (i.e. script)
    // loaded with certificate errors.
    RAN_CONTENT_WITH_CERT_ERRORS = 1 << 3,

    // HTTP page containing a password input. Embedders may use this to
    // adjust UI on nonsecure pages that collect sensitive data.
    // TODO: integrate password detection to set this flag.
    // https://crbug.com/647560
    DISPLAYED_PASSWORD_FIELD_ON_HTTP = 1 << 4,

    // HTTP page containing a credit card input. Embedders may use this to
    // adjust UI on nonsecure pages that collect sensitive data.
    // TODO: integrate credit card detection to set this flag.
    // https://crbug.com/647560
    DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP = 1 << 5,
  };

  SSLStatus();
  SSLStatus(const net::SSLInfo& ssl_info);
  SSLStatus(const SSLStatus& other);
  ~SSLStatus();

  bool Equals(const SSLStatus& status) const {
    return initialized == status.initialized &&
           !!certificate == !!status.certificate &&
           (certificate ? certificate->Equals(status.certificate.get())
                        : true) &&
           cert_status == status.cert_status &&
           security_bits == status.security_bits &&
           key_exchange_group == status.key_exchange_group &&
           connection_status == status.connection_status &&
           content_status == status.content_status &&
           sct_statuses == status.sct_statuses &&
           pkp_bypassed == status.pkp_bypassed;
  }

  bool initialized;
  scoped_refptr<net::X509Certificate> certificate;
  net::CertStatus cert_status;
  int security_bits;
  uint16_t key_exchange_group;
  int connection_status;
  // A combination of the ContentStatusFlags above.
  int content_status;
  // The validation statuses of the Signed Certificate Timestamps (SCTs)
  // of Certificate Transparency (CT) that were served with the
  // main resource.
  std::vector<net::ct::SCTVerifyStatus> sct_statuses;
  // True if PKP was bypassed due to a local trust anchor.
  bool pkp_bypassed;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SSL_STATUS_H_
