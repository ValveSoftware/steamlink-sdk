// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/metrics_util.h"

#include <string.h>

#include "base/sha1.h"
#include "base/sys_byteorder.h"

namespace metrics {

uint32_t HashName(const std::string& name) {
  // SHA-1 is designed to produce a uniformly random spread in its output space,
  // even for nearly-identical inputs.
  unsigned char sha1_hash[base::kSHA1Length];
  base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(name.c_str()),
                      name.size(),
                      sha1_hash);

  uint32_t bits;
  static_assert(sizeof(bits) < sizeof(sha1_hash), "more data required");
  memcpy(&bits, sha1_hash, sizeof(bits));

  return base::ByteSwapToLE32(bits);
}

}  // namespace metrics
