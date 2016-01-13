// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SSL_STATUS_H_
#define CONTENT_PUBLIC_COMMON_SSL_STATUS_H_

#include "content/common/content_export.h"
#include "content/public/common/security_style.h"
#include "content/public/common/signed_certificate_timestamp_id_and_status.h"
#include "net/cert/cert_status_flags.h"

namespace content {

// Collects the SSL information for this NavigationEntry.
struct CONTENT_EXPORT SSLStatus {
  // Flags used for the page security content status.
  enum ContentStatusFlags {
    // HTTP page, or HTTPS page with no insecure content.
    NORMAL_CONTENT             = 0,

    // HTTPS page containing "displayed" HTTP resources (e.g. images, CSS).
    DISPLAYED_INSECURE_CONTENT = 1 << 0,

    // HTTPS page containing "executed" HTTP resources (i.e. script).
    // Also currently used for HTTPS page containing broken-HTTPS resources;
    // this is wrong and should be fixed (see comments in
    // SSLPolicy::OnRequestStarted()).
    RAN_INSECURE_CONTENT       = 1 << 1,
  };

  SSLStatus();
  ~SSLStatus();

  bool Equals(const SSLStatus& status) const {
    return security_style == status.security_style &&
           cert_id == status.cert_id &&
           cert_status == status.cert_status &&
           security_bits == status.security_bits &&
           content_status == status.content_status &&
           signed_certificate_timestamp_ids ==
               status.signed_certificate_timestamp_ids;
  }

  content::SecurityStyle security_style;
  int cert_id;
  net::CertStatus cert_status;
  int security_bits;
  int connection_status;
  // A combination of the ContentStatusFlags above.
  int content_status;
  SignedCertificateTimestampIDStatusList signed_certificate_timestamp_ids;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SSL_STATUS_H_
