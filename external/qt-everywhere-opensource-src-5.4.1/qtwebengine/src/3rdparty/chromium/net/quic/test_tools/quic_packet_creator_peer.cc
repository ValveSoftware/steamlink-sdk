// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_packet_creator_peer.h"

#include "net/quic/quic_packet_creator.h"

namespace net {
namespace test {

// static
bool QuicPacketCreatorPeer::SendVersionInPacket(QuicPacketCreator* creator) {
  return creator->send_version_in_packet_;
}

// static
void QuicPacketCreatorPeer::SetSendVersionInPacket(
    QuicPacketCreator* creator, bool send_version_in_packet) {
  creator->send_version_in_packet_ = send_version_in_packet;
}

// static
void QuicPacketCreatorPeer::SetSequenceNumberLength(
      QuicPacketCreator* creator,
      QuicSequenceNumberLength sequence_number_length) {
  creator->sequence_number_length_ = sequence_number_length;
}

// static
QuicSequenceNumberLength QuicPacketCreatorPeer::GetSequenceNumberLength(
    QuicPacketCreator* creator) {
  return creator->sequence_number_length_;
}

}  // namespace test
}  // namespace net
