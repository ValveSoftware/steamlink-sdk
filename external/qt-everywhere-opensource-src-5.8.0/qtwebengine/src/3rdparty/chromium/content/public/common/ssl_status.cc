// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/ssl_status.h"

#include "net/cert/sct_status_flags.h"
#include "net/ssl/ssl_info.h"

namespace content {

SSLStatus::SSLStatus()
    : security_style(SECURITY_STYLE_UNKNOWN),
      cert_id(0),
      cert_status(0),
      security_bits(-1),
      key_exchange_info(0),
      connection_status(0),
      content_status(NORMAL_CONTENT),
      num_unknown_scts(0),
      num_invalid_scts(0),
      num_valid_scts(0),
      pkp_bypassed(false) {}

SSLStatus::SSLStatus(SecurityStyle security_style,
                     int cert_id,
                     const net::SSLInfo& ssl_info)
    : security_style(security_style),
      cert_id(cert_id),
      cert_status(ssl_info.cert_status),
      security_bits(ssl_info.security_bits),
      key_exchange_info(ssl_info.key_exchange_info),
      connection_status(ssl_info.connection_status),
      content_status(NORMAL_CONTENT),
      num_unknown_scts(0),
      num_invalid_scts(0),
      num_valid_scts(0),
      pkp_bypassed(ssl_info.pkp_bypassed) {
  // Count unknown, invalid and valid SCTs.
  for (const auto& sct_and_status : ssl_info.signed_certificate_timestamps) {
    switch (sct_and_status.status) {
      case net::ct::SCT_STATUS_LOG_UNKNOWN:
        num_unknown_scts++;
        break;
      case net::ct::SCT_STATUS_INVALID:
        num_invalid_scts++;
        break;
      case net::ct::SCT_STATUS_OK:
        num_valid_scts++;
        break;
      case net::ct::SCT_STATUS_NONE:
      case net::ct::SCT_STATUS_MAX:
        // These enum values do not represent SCTs that are taken into account
        // for CT compliance calculations, so we ignore them.
        NOTREACHED();
        break;
    }
  }
}

SSLStatus::SSLStatus(const SSLStatus& other) = default;

SSLStatus::~SSLStatus() {}

}  // namespace content
