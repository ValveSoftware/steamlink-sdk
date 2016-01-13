// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SIGNED_CERTIFICATE_TIMESTAMP_ID_AND_STATUS_H_
#define CONTENT_PUBLIC_COMMON_SIGNED_CERTIFICATE_TIMESTAMP_ID_AND_STATUS_H_

#include <vector>

#include "content/common/content_export.h"
#include "net/cert/sct_status_flags.h"

namespace content {

// Holds the ID of a SignedCertificateTimestamp (as assigned by
// SignedCertificateTimestampStore), and its verification status.
struct CONTENT_EXPORT SignedCertificateTimestampIDAndStatus {
  SignedCertificateTimestampIDAndStatus(
      int id, net::ct::SCTVerifyStatus status);

  bool operator==(const SignedCertificateTimestampIDAndStatus& other) const;

  int id;
  net::ct::SCTVerifyStatus status;
};

typedef std::vector<SignedCertificateTimestampIDAndStatus>
    SignedCertificateTimestampIDStatusList;

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SIGNED_CERTIFICATE_TIMESTAMP_ID_AND_STATUS_H_
