// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_SIMPLE_QUIC_FRAMER_H_
#define NET_QUIC_TEST_TOOLS_SIMPLE_QUIC_FRAMER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

class CryptoHandshakeMessage;
struct QuicAckFrame;
class QuicConnection;
class QuicConnectionVisitorInterface;
class QuicPacketCreator;
class ReceiveAlgorithmInterface;
class SendAlgorithmInterface;

namespace test {

class SimpleFramerVisitor;

// Peer to make public a number of otherwise private QuicFramer methods.
class SimpleQuicFramer {
 public:
  SimpleQuicFramer();
  explicit SimpleQuicFramer(const QuicVersionVector& supported_versions);
  ~SimpleQuicFramer();

  bool ProcessPacket(const QuicEncryptedPacket& packet);
  bool ProcessPacket(const QuicPacket& packet);
  void Reset();

  const QuicPacketHeader& header() const;
  size_t num_frames() const;
  const std::vector<QuicAckFrame>& ack_frames() const;
  const std::vector<QuicConnectionCloseFrame>& connection_close_frames() const;
  const std::vector<QuicCongestionFeedbackFrame>& feedback_frames() const;
  const std::vector<QuicStopWaitingFrame>& stop_waiting_frames() const;
  const std::vector<QuicPingFrame>& ping_frames() const;
  const std::vector<QuicGoAwayFrame>& goaway_frames() const;
  const std::vector<QuicRstStreamFrame>& rst_stream_frames() const;
  const std::vector<QuicStreamFrame>& stream_frames() const;
  const QuicFecData& fec_data() const;
  const QuicVersionNegotiationPacket* version_negotiation_packet() const;
  const QuicPublicResetPacket* public_reset_packet() const;

  QuicFramer* framer();

  void SetSupportedVersions(const QuicVersionVector& versions) {
    framer_.SetSupportedVersions(versions);
  }

 private:
  QuicFramer framer_;
  scoped_ptr<SimpleFramerVisitor> visitor_;
  DISALLOW_COPY_AND_ASSIGN(SimpleQuicFramer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_SIMPLE_QUIC_FRAMER_H_
