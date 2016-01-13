// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Manages the packet entropy calculation for both sent and received packets
// for a connection.

#ifndef NET_QUIC_QUIC_SENT_ENTROPY_MANAGER_H_
#define NET_QUIC_QUIC_SENT_ENTROPY_MANAGER_H_

#include "net/base/linked_hash_map.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

// Records all sent packets by a connection to track the cumulative entropy of
// sent packets.  It is used by the connection to validate an ack
// frame sent by the peer as a preventive measure against the optimistic ack
// attack.
class NET_EXPORT_PRIVATE QuicSentEntropyManager {
 public:
  QuicSentEntropyManager();
  virtual ~QuicSentEntropyManager();

  // Record |entropy_hash| for sent packet corresponding to |sequence_number|.
  void RecordPacketEntropyHash(QuicPacketSequenceNumber sequence_number,
                               QuicPacketEntropyHash entropy_hash);

  QuicPacketEntropyHash EntropyHash(
      QuicPacketSequenceNumber sequence_number) const;

  // Returns true if |entropy_hash| matches the expected sent entropy hash
  // up to |sequence_number| removing sequence numbers from |missing_packets|.
  bool IsValidEntropy(QuicPacketSequenceNumber sequence_number,
                      const SequenceNumberSet& missing_packets,
                      QuicPacketEntropyHash entropy_hash) const;

  // Removes not required entries from |packets_entropy_| before
  // |sequence_number|.
  void ClearEntropyBefore(QuicPacketSequenceNumber sequence_number);

 private:
  typedef linked_hash_map<QuicPacketSequenceNumber,
                          std::pair<QuicPacketEntropyHash,
                               QuicPacketEntropyHash> > SentEntropyMap;

  // Linked hash map from sequence numbers to the sent entropy hash up to the
  // sequence number in the key.
  SentEntropyMap packets_entropy_;

  // Cumulative hash of entropy of all sent packets.
  QuicPacketEntropyHash packets_entropy_hash_;

  DISALLOW_COPY_AND_ASSIGN(QuicSentEntropyManager);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SENT_ENTROPY_MANAGER_H_
