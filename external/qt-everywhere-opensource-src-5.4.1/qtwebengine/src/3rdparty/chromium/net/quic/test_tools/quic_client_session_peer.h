// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_CLIENT_SESSION_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_CLIENT_SESSION_PEER_H_

#include "net/quic/quic_protocol.h"

namespace net {

class QuicClientSession;

namespace test {

class QuicClientSessionPeer {
 public:
  static void SetMaxOpenStreams(QuicClientSession* session,
                                size_t max_streams,
                                size_t default_streams);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicClientSessionPeer);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_CLIENT_SESSION_PEER_H_
