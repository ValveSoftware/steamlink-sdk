// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/signed_certificate_timestamp_id_and_status.h"

namespace content {

SignedCertificateTimestampIDAndStatus::SignedCertificateTimestampIDAndStatus(
    int id, net::ct::SCTVerifyStatus status)
    : id(id), status(status) {}

bool SignedCertificateTimestampIDAndStatus::operator==(
    const SignedCertificateTimestampIDAndStatus& other) const {
  return id == other.id && status == other.status;
}

}  // namespace content
