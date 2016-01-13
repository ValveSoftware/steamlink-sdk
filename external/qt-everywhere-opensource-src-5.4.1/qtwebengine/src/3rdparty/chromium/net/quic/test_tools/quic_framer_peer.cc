// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_framer_peer.h"

#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {
namespace test {

// static
QuicPacketSequenceNumber QuicFramerPeer::CalculatePacketSequenceNumberFromWire(
    QuicFramer* framer,
    QuicSequenceNumberLength sequence_number_length,
    QuicPacketSequenceNumber packet_sequence_number) {
  return framer->CalculatePacketSequenceNumberFromWire(sequence_number_length,
                                                       packet_sequence_number);
}

// static
void QuicFramerPeer::SetLastSerializedConnectionId(
    QuicFramer* framer, QuicConnectionId connection_id) {
  framer->last_serialized_connection_id_ = connection_id;
}

void QuicFramerPeer::SetLastSequenceNumber(
    QuicFramer* framer,
    QuicPacketSequenceNumber packet_sequence_number) {
  framer->last_sequence_number_ = packet_sequence_number;
}

void QuicFramerPeer::SetIsServer(QuicFramer* framer, bool is_server) {
  framer->is_server_ = is_server;
}

void QuicFramerPeer::SwapCrypters(QuicFramer* framer1, QuicFramer* framer2) {
  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; i++) {
    framer1->encrypter_[i].swap(framer2->encrypter_[i]);
  }
  framer1->decrypter_.swap(framer2->decrypter_);
  framer1->alternative_decrypter_.swap(framer2->alternative_decrypter_);

  EncryptionLevel framer2_level = framer2->decrypter_level_;
  framer2->decrypter_level_ = framer1->decrypter_level_;
  framer1->decrypter_level_ = framer2_level;
  framer2_level = framer2->alternative_decrypter_level_;
  framer2->alternative_decrypter_level_ =
      framer1->alternative_decrypter_level_;
  framer1->alternative_decrypter_level_ = framer2_level;

  const bool framer2_latch = framer2->alternative_decrypter_latch_;
  framer2->alternative_decrypter_latch_ =
      framer1->alternative_decrypter_latch_;
  framer1->alternative_decrypter_latch_ = framer2_latch;
}

}  // namespace test
}  // namespace net
