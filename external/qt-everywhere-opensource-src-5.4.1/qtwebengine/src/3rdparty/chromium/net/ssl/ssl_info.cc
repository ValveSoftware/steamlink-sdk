// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_info.h"

#include "base/pickle.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_certificate.h"

namespace net {

SSLInfo::SSLInfo() {
  Reset();
}

SSLInfo::SSLInfo(const SSLInfo& info) {
  *this = info;
}

SSLInfo::~SSLInfo() {
}

SSLInfo& SSLInfo::operator=(const SSLInfo& info) {
  cert = info.cert;
  cert_status = info.cert_status;
  security_bits = info.security_bits;
  connection_status = info.connection_status;
  is_issued_by_known_root = info.is_issued_by_known_root;
  client_cert_sent = info.client_cert_sent;
  channel_id_sent = info.channel_id_sent;
  handshake_type = info.handshake_type;
  public_key_hashes = info.public_key_hashes;
  signed_certificate_timestamps = info.signed_certificate_timestamps;
  pinning_failure_log = info.pinning_failure_log;

  return *this;
}

void SSLInfo::Reset() {
  cert = NULL;
  cert_status = 0;
  security_bits = -1;
  connection_status = 0;
  is_issued_by_known_root = false;
  client_cert_sent = false;
  channel_id_sent = false;
  handshake_type = HANDSHAKE_UNKNOWN;
  public_key_hashes.clear();
  signed_certificate_timestamps.clear();
  pinning_failure_log.clear();
}

void SSLInfo::SetCertError(int error) {
  cert_status |= MapNetErrorToCertStatus(error);
}

}  // namespace net
