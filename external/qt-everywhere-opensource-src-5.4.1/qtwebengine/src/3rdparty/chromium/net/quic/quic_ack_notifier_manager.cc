// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_ack_notifier_manager.h"

#include <stddef.h>
#include <list>
#include <map>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_protocol.h"

namespace net {

AckNotifierManager::AckNotifierManager() {}

AckNotifierManager::~AckNotifierManager() {
  STLDeleteElements(&ack_notifiers_);
}

void AckNotifierManager::OnPacketAcked(
    QuicPacketSequenceNumber sequence_number,
    QuicTime::Delta delta_largest_observed) {
  // Inform all the registered AckNotifiers of the new ACK.
  AckNotifierMap::iterator map_it = ack_notifier_map_.find(sequence_number);
  if (map_it == ack_notifier_map_.end()) {
    // No AckNotifier is interested in this sequence number.
    return;
  }

  // One or more AckNotifiers are registered as interested in this sequence
  // number. Iterate through them and call OnAck on each.
  for (AckNotifierSet::iterator set_it = map_it->second.begin();
       set_it != map_it->second.end(); ++set_it) {
    QuicAckNotifier* ack_notifier = *set_it;
    ack_notifier->OnAck(sequence_number, delta_largest_observed);

    // If this has resulted in an empty AckNotifer, erase it.
    if (ack_notifier->IsEmpty()) {
      delete ack_notifier;
      ack_notifiers_.erase(ack_notifier);
    }
  }

  // Remove the sequence number from the map as we have notified all the
  // registered AckNotifiers, and we won't see it again.
  ack_notifier_map_.erase(map_it);
}

void AckNotifierManager::UpdateSequenceNumber(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number) {
  AckNotifierMap::iterator map_it = ack_notifier_map_.find(old_sequence_number);
  if (map_it != ack_notifier_map_.end()) {
    // We will add an entry to the map for the new sequence number, and move
    // the
    // list of AckNotifiers over.
    AckNotifierSet new_set;
    for (AckNotifierSet::iterator notifier_it = map_it->second.begin();
         notifier_it != map_it->second.end(); ++notifier_it) {
      (*notifier_it)
          ->UpdateSequenceNumber(old_sequence_number, new_sequence_number);
      new_set.insert(*notifier_it);
    }
    ack_notifier_map_[new_sequence_number] = new_set;
    ack_notifier_map_.erase(map_it);
  }
}

void AckNotifierManager::OnSerializedPacket(
    const SerializedPacket& serialized_packet) {
  // Run through all the frames and if any of them are stream frames and have
  // an AckNotifier registered, then inform the AckNotifier that it should be
  // interested in this packet's sequence number.

  RetransmittableFrames* frames = serialized_packet.retransmittable_frames;

  // AckNotifiers can only be attached to retransmittable frames.
  if (!frames) {
    return;
  }

  for (QuicFrames::const_iterator it = frames->frames().begin();
       it != frames->frames().end(); ++it) {
    if (it->type == STREAM_FRAME && it->stream_frame->notifier != NULL) {
      QuicAckNotifier* notifier = it->stream_frame->notifier;

      // The AckNotifier needs to know it is tracking this packet's sequence
      // number.
      notifier->AddSequenceNumber(serialized_packet.sequence_number,
                                  serialized_packet.packet->length());

      // Update the mapping in the other direction, from sequence
      // number to AckNotifier.
      ack_notifier_map_[serialized_packet.sequence_number].insert(notifier);

      // Take ownership of the AckNotifier.
      ack_notifiers_.insert(notifier);
    }
  }
}

}  // namespace net
