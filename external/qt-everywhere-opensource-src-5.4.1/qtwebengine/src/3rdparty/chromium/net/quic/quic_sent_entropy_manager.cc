// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_entropy_manager.h"

#include "base/logging.h"
#include "net/base/linked_hash_map.h"

using std::make_pair;
using std::max;
using std::min;

namespace net {

QuicSentEntropyManager::QuicSentEntropyManager()
    : packets_entropy_hash_(0) {}

QuicSentEntropyManager::~QuicSentEntropyManager() {}

void QuicSentEntropyManager::RecordPacketEntropyHash(
    QuicPacketSequenceNumber sequence_number,
    QuicPacketEntropyHash entropy_hash) {
  // TODO(satyamshekhar): Check this logic again when/if we enable packet
  // reordering.
  packets_entropy_hash_ ^= entropy_hash;
  packets_entropy_.insert(
      make_pair(sequence_number,
                make_pair(entropy_hash, packets_entropy_hash_)));
  DVLOG(2) << "setting cumulative sent entropy hash to: "
           << static_cast<int>(packets_entropy_hash_)
           << " updated with sequence number " << sequence_number
           << " entropy hash: " << static_cast<int>(entropy_hash);
}

QuicPacketEntropyHash QuicSentEntropyManager::EntropyHash(
    QuicPacketSequenceNumber sequence_number) const {
  SentEntropyMap::const_iterator it =
      packets_entropy_.find(sequence_number);
  if (it == packets_entropy_.end()) {
    // Should only happen when we have not received ack for any packet.
    DCHECK_EQ(0u, sequence_number);
    return 0;
  }
  return it->second.second;
}

bool QuicSentEntropyManager::IsValidEntropy(
    QuicPacketSequenceNumber sequence_number,
    const SequenceNumberSet& missing_packets,
    QuicPacketEntropyHash entropy_hash) const {
  SentEntropyMap::const_iterator entropy_it =
      packets_entropy_.find(sequence_number);
  if (entropy_it == packets_entropy_.end()) {
    DCHECK_EQ(0u, sequence_number);
    // Close connection if something goes wrong.
    return 0 == sequence_number;
  }
  QuicPacketEntropyHash expected_entropy_hash = entropy_it->second.second;
  for (SequenceNumberSet::const_iterator it = missing_packets.begin();
       it != missing_packets.end(); ++it) {
    entropy_it = packets_entropy_.find(*it);
    DCHECK(entropy_it != packets_entropy_.end());
    expected_entropy_hash ^= entropy_it->second.first;
  }
  DLOG_IF(WARNING, entropy_hash != expected_entropy_hash)
      << "Invalid entropy hash: " << static_cast<int>(entropy_hash)
      << " expected entropy hash: " << static_cast<int>(expected_entropy_hash);
  return entropy_hash == expected_entropy_hash;
}

void QuicSentEntropyManager::ClearEntropyBefore(
    QuicPacketSequenceNumber sequence_number) {
  if (packets_entropy_.empty()) {
    return;
  }
  SentEntropyMap::iterator it = packets_entropy_.begin();
  while (it->first < sequence_number) {
    packets_entropy_.erase(it);
    it = packets_entropy_.begin();
    DCHECK(it != packets_entropy_.end());
  }
  DVLOG(2) << "Cleared entropy before: "
           << packets_entropy_.begin()->first;
}

}  // namespace net
