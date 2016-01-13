// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_SPDY_UTILS_H_
#define NET_QUIC_SPDY_UTILS_H_

#include <string>

#include "net/base/net_export.h"
#include "net/spdy/spdy_framer.h"

namespace net {

class NET_EXPORT_PRIVATE SpdyUtils {
 public:
  static std::string SerializeUncompressedHeaders(
      const SpdyHeaderBlock& headers);

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyUtils);
};

}  // namespace net

#endif  // NET_QUIC_SPDY_UTILS_H_
