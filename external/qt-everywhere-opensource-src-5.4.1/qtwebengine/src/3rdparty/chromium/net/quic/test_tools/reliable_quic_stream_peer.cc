// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/reliable_quic_stream_peer.h"

#include <list>

#include "net/quic/reliable_quic_stream.h"

namespace net {
namespace test {

// static
void ReliableQuicStreamPeer::SetWriteSideClosed(bool value,
                                                ReliableQuicStream* stream) {
  stream->write_side_closed_ = value;
}

// static
void ReliableQuicStreamPeer::SetStreamBytesWritten(
    QuicStreamOffset stream_bytes_written,
    ReliableQuicStream* stream) {
  stream->stream_bytes_written_ = stream_bytes_written;
}

// static
void ReliableQuicStreamPeer::CloseReadSide(ReliableQuicStream* stream) {
  stream->CloseReadSide();
}

// static
bool ReliableQuicStreamPeer::FinSent(ReliableQuicStream* stream) {
  return stream->fin_sent_;
}

// static
bool ReliableQuicStreamPeer::RstSent(ReliableQuicStream* stream) {
  return stream->rst_sent_;
}

// static
uint32 ReliableQuicStreamPeer::SizeOfQueuedData(ReliableQuicStream* stream) {
  uint32 total = 0;
  std::list<ReliableQuicStream::PendingData>::iterator it =
      stream->queued_data_.begin();
  while (it != stream->queued_data_.end()) {
    total += it->data.size();
    ++it;
  }
  return total;
}

// static
void ReliableQuicStreamPeer::SetFecPolicy(ReliableQuicStream* stream,
                                          FecPolicy fec_policy) {
  stream->set_fec_policy(fec_policy);
}

}  // namespace test
}  // namespace net
