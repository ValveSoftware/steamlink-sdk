// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ssl_status.h"

#include "net/cert/sct_status_flags.h"
#include "net/ssl/ssl_info.h"

namespace content {

SSLStatus::SSLStatus()
    : initialized(false),
      cert_status(0),
      security_bits(-1),
      key_exchange_group(0),
      connection_status(0),
      content_status(NORMAL_CONTENT),
      pkp_bypassed(false) {}

SSLStatus::SSLStatus(const net::SSLInfo& ssl_info)
    : initialized(true),
      certificate(ssl_info.cert),
      cert_status(ssl_info.cert_status),
      security_bits(ssl_info.security_bits),
      key_exchange_group(ssl_info.key_exchange_group),
      connection_status(ssl_info.connection_status),
      content_status(NORMAL_CONTENT),
      pkp_bypassed(ssl_info.pkp_bypassed) {
  for (const auto& sct_and_status : ssl_info.signed_certificate_timestamps) {
    sct_statuses.push_back(sct_and_status.status);
  }
}

SSLStatus::SSLStatus(const SSLStatus& other) = default;

SSLStatus::~SSLStatus() {}

}  // namespace content
