// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_session_peer.h"

#include "net/quic/quic_session.h"
#include "net/quic/reliable_quic_stream.h"

namespace net {
namespace test {

// static
void QuicSessionPeer::SetNextStreamId(QuicSession* session, QuicStreamId id) {
  session->next_stream_id_ = id;
}

// static
void QuicSessionPeer::SetMaxOpenStreams(QuicSession* session,
                                        uint32 max_streams) {
  session->max_open_streams_ = max_streams;
}

// static
QuicHeadersStream* QuicSessionPeer::GetHeadersStream(QuicSession* session) {
  return session->headers_stream_.get();
}

// static
void QuicSessionPeer::SetHeadersStream(QuicSession* session,
                                       QuicHeadersStream* headers_stream) {
  session->headers_stream_.reset(headers_stream);
}

// static
QuicWriteBlockedList* QuicSessionPeer::GetWriteBlockedStreams(
    QuicSession* session) {
  return &session->write_blocked_streams_;
}

// static
QuicDataStream* QuicSessionPeer::GetIncomingDataStream(
    QuicSession* session,
    QuicStreamId stream_id) {
  return session->GetIncomingDataStream(stream_id);
}

}  // namespace test
}  // namespace net
