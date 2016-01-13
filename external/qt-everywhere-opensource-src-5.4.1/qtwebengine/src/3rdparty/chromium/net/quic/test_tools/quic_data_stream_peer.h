// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_DATA_STREAM_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_DATA_STREAM_PEER_H_

#include "base/basictypes.h"

namespace net {

class QuicDataStream;

namespace test {

class QuicDataStreamPeer {
 public:
  static void SetHeadersDecompressed(QuicDataStream* stream,
                                     bool headers_decompressed);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicDataStreamPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_DATA_STREAM_PEER_H_
