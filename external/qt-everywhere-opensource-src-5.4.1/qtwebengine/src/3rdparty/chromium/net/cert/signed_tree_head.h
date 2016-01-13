// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_SIGNED_TREE_HEAD_H_
#define NET_CERT_SIGNED_TREE_HEAD_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/cert/signed_certificate_timestamp.h"

namespace net {

namespace ct {

static const uint8 kSthRootHashLength = 32;

// Signed Tree Head as defined in section 3.5. of RFC6962
struct NET_EXPORT SignedTreeHead {
  // Version enum in RFC 6962, Section 3.2. Note that while in the current
  // RFC the STH and SCT share the versioning scheme, there are plans in
  // RFC6962-bis to use separate versions, so using a separate scheme here.
  enum Version { V1 = 0, };

  Version version;
  base::Time timestamp;
  uint64 tree_size;
  char sha256_root_hash[kSthRootHashLength];
  DigitallySigned signature;
};

}  // namespace ct

}  // namespace net

#endif
