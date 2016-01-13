// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_PACKET_CREATOR_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_PACKET_CREATOR_PEER_H_

#include "net/quic/quic_protocol.h"

namespace net {
class QuicPacketCreator;

namespace test {

class QuicPacketCreatorPeer {
 public:
  static bool SendVersionInPacket(QuicPacketCreator* creator);

  static void SetSendVersionInPacket(QuicPacketCreator* creator,
                                     bool send_version_in_packet);
  static void SetSequenceNumberLength(
      QuicPacketCreator* creator,
      QuicSequenceNumberLength sequence_number_length);
  static QuicSequenceNumberLength GetSequenceNumberLength(
      QuicPacketCreator* creator);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicPacketCreatorPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_PACKET_CREATOR_PEER_H_
