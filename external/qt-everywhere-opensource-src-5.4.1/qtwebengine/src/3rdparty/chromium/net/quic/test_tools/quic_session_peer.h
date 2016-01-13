// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_SESSION_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_SESSION_PEER_H_

#include "net/quic/quic_protocol.h"
#include "net/quic/quic_write_blocked_list.h"

namespace net {

class QuicDataStream;
class QuicHeadersStream;
class QuicSession;

namespace test {

class QuicSessionPeer {
 public:
  static void SetNextStreamId(QuicSession* session, QuicStreamId id);
  static void SetMaxOpenStreams(QuicSession* session, uint32 max_streams);
  static QuicHeadersStream* GetHeadersStream(QuicSession* session);
  static void SetHeadersStream(QuicSession* session,
                               QuicHeadersStream* headers_stream);
  static QuicWriteBlockedList* GetWriteBlockedStreams(QuicSession* session);
  static QuicDataStream* GetIncomingDataStream(QuicSession* session,
                                               QuicStreamId stream_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicSessionPeer);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_SESSION_PEER_H_
