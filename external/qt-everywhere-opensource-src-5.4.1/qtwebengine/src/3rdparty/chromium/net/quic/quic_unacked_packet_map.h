// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_UNACKED_PACKET_MAP_H_
#define NET_QUIC_QUIC_UNACKED_PACKET_MAP_H_

#include "net/base/linked_hash_map.h"
#include "net/quic/quic_protocol.h"

namespace net {

// Class which tracks unacked packets for three purposes:
// 1) Track retransmittable data, including multiple transmissions of frames.
// 2) Track packets and bytes in flight for congestion control.
// 3) Track sent time of packets to provide RTT measurements from acks.
class NET_EXPORT_PRIVATE QuicUnackedPacketMap {
 public:
  QuicUnackedPacketMap();
  ~QuicUnackedPacketMap();

  // Adds |serialized_packet| to the map.  Does not mark it in flight.
  void AddPacket(const SerializedPacket& serialized_packet);

  // Called when a packet is retransmitted with a new sequence number.
  // |old_sequence_number| will remain unacked, but will have no
  // retransmittable data associated with it. |new_sequence_number| will
  // be both unacked and associated with retransmittable data.
  void OnRetransmittedPacket(QuicPacketSequenceNumber old_sequence_number,
                             QuicPacketSequenceNumber new_sequence_number,
                             TransmissionType transmission_type);

  // Returns true if the packet |sequence_number| is unacked.
  bool IsUnacked(QuicPacketSequenceNumber sequence_number) const;

  // Sets the nack count to the max of the current nack count and |min_nacks|.
  void NackPacket(QuicPacketSequenceNumber sequence_number,
                  size_t min_nacks);

  // Marks |sequence_number| as no longer in flight.
  void RemoveFromInFlight(QuicPacketSequenceNumber sequence_number);

  // Returns true if the unacked packet |sequence_number| has retransmittable
  // frames.  This will return false if the packet has been acked, if a
  // previous transmission of this packet was ACK'd, or if this packet has been
  // retransmitted as with different sequence number, or if the packet never
  // had any retransmittable packets in the first place.
  bool HasRetransmittableFrames(QuicPacketSequenceNumber sequence_number) const;

  // Returns true if there are any unacked packets.
  bool HasUnackedPackets() const;

  // Returns true if there are any unacked packets which have retransmittable
  // frames.
  bool HasUnackedRetransmittableFrames() const;

  // Returns the largest sequence number that has been sent.
  QuicPacketSequenceNumber largest_sent_packet() const {
    return largest_sent_packet_;
  }

  // Returns the sum of bytes from all packets in flight.
  QuicByteCount bytes_in_flight() const {
    return bytes_in_flight_;
  }

  // Returns the smallest sequence number of a serialized packet which has not
  // been acked by the peer.  If there are no unacked packets, returns 0.
  QuicPacketSequenceNumber GetLeastUnackedSentPacket() const;

  // Sets a packet as sent with the sent time |sent_time|.  Marks the packet
  // as in flight if |set_in_flight| is true.
  // Packets marked as in flight are expected to be marked as missing when they
  // don't arrive, indicating the need for retransmission.
  void SetSent(QuicPacketSequenceNumber sequence_number,
               QuicTime sent_time,
               QuicByteCount bytes_sent,
               bool set_in_flight);

  // Clears up to |num_to_clear| previous transmissions in order to make room
  // in the ack frame for new acks.
  void ClearPreviousRetransmissions(size_t num_to_clear);

  typedef linked_hash_map<QuicPacketSequenceNumber,
                          TransmissionInfo> UnackedPacketMap;

  typedef UnackedPacketMap::const_iterator const_iterator;

  const_iterator begin() const { return unacked_packets_.begin(); }
  const_iterator end() const { return unacked_packets_.end(); }

  // Returns true if there are unacked packets that are in flight.
  bool HasInFlightPackets() const;

  // Returns the TransmissionInfo associated with |sequence_number|, which
  // must be unacked.
  const TransmissionInfo& GetTransmissionInfo(
      QuicPacketSequenceNumber sequence_number) const;

  // Returns the time that the last unacked packet was sent.
  QuicTime GetLastPacketSentTime() const;

  // Returns the time that the first in flight packet was sent.
  QuicTime GetFirstInFlightPacketSentTime() const;

  // Returns the number of unacked packets.
  size_t GetNumUnackedPackets() const;

  // Returns true if there are multiple packets in flight.
  bool HasMultipleInFlightPackets() const;

  // Returns true if there are any pending crypto packets.
  bool HasPendingCryptoPackets() const;

  // Removes any retransmittable frames from this transmission or an associated
  // transmission.  It removes now useless transmissions, and disconnects any
  // other packets from other transmissions.
  void RemoveRetransmittability(QuicPacketSequenceNumber sequence_number);

  // Increases the largest observed.  Any packets less or equal to
  // |largest_acked_packet| are discarded if they are only for the RTT purposes.
  void IncreaseLargestObserved(QuicPacketSequenceNumber largest_observed);

 private:
  void MaybeRemoveRetransmittableFrames(TransmissionInfo* transmission_info);

  // Returns true if the packet no longer has a purpose in the map.
  bool IsPacketUseless(UnackedPacketMap::const_iterator it) const;

  QuicPacketSequenceNumber largest_sent_packet_;
  QuicPacketSequenceNumber largest_observed_;

  // Newly serialized retransmittable and fec packets are added to this map,
  // which contains owning pointers to any contained frames.  If a packet is
  // retransmitted, this map will contain entries for both the old and the new
  // packet. The old packet's retransmittable frames entry will be NULL, while
  // the new packet's entry will contain the frames to retransmit.
  // If the old packet is acked before the new packet, then the old entry will
  // be removed from the map and the new entry's retransmittable frames will be
  // set to NULL.
  UnackedPacketMap unacked_packets_;

  size_t bytes_in_flight_;
  // Number of retransmittable crypto handshake packets.
  size_t pending_crypto_packet_count_;

  DISALLOW_COPY_AND_ASSIGN(QuicUnackedPacketMap);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_UNACKED_PACKET_MAP_H_
