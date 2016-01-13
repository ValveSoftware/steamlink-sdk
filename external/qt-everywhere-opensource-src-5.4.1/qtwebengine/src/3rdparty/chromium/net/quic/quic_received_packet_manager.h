// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Manages the packet entropy calculation for both sent and received packets
// for a connection.

#ifndef NET_QUIC_QUIC_RECEIVED_PACKET_MANAGER_H_
#define NET_QUIC_QUIC_RECEIVED_PACKET_MANAGER_H_

#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

namespace test {
class EntropyTrackerPeer;
class QuicConnectionPeer;
class QuicReceivedPacketManagerPeer;
}  // namespace test

struct QuicConnectionStats;

// Records all received packets by a connection and tracks their entropy.
// Also calculates the correct entropy for the framer when it truncates an ack
// frame being serialized.
class NET_EXPORT_PRIVATE QuicReceivedPacketManager :
    public QuicReceivedEntropyHashCalculatorInterface {
 public:
  class NET_EXPORT_PRIVATE EntropyTracker {
   public:
    EntropyTracker();
    ~EntropyTracker();

    // Compute the XOR of the entropy of all received packets up to
    // and including sequence_number.
    // Requires that either:
    //   sequence_number == largest_observed_
    // or:
    //   sequence_number > first_gap_ &&
    //   sequence_number < largest_observed_ &&
    //   sequence_number in packets_entropy_
    QuicPacketEntropyHash EntropyHash(
        QuicPacketSequenceNumber sequence_number) const;

    // Record the received entropy hash against |sequence_number|.
    // Performs garbage collection to advance first_gap_ if
    // sequence_number == first_gap_.
    void RecordPacketEntropyHash(QuicPacketSequenceNumber sequence_number,
                                 QuicPacketEntropyHash entropy_hash);

    // Sets the entropy hash up to but not including a sequence number based
    // on the hash provided by a StopWaiting frame.  Clears older packet
    // entropy entries and performs garbage collection up to the first gap.
    void SetCumulativeEntropyUpTo(QuicPacketSequenceNumber sequence_number,
                                  QuicPacketEntropyHash entropy_hash);

   private:
    friend class test::EntropyTrackerPeer;

    typedef std::map<QuicPacketSequenceNumber,
                     QuicPacketEntropyHash> ReceivedEntropyMap;

    // Recomputes first_gap_ and removes packets_entropy_ entries that are no
    // longer needed to compute EntropyHash.
    void AdvanceFirstGapAndGarbageCollectEntropyMap();

    // TODO(satyamshekhar): Can be optimized using an interval set like data
    // structure.
    // Map of received sequence numbers to their corresponding entropy.
    // Stores an entry for every received packet whose sequence_number is larger
    // than first_gap_.  Packets without the entropy bit set have an entropy
    // value of 0.
    // TODO(ianswett): When the entropy flag is off, the entropy
    // should not be 0.
    ReceivedEntropyMap packets_entropy_;

    // Cumulative hash of entropy of all received packets.
    QuicPacketEntropyHash packets_entropy_hash_;

    // Sequence number of the first packet that we do not know the entropy of.
    // If there are no gaps in the received packet sequence,
    // packets_entropy_ will be empty and first_gap_ will be equal to
    // 'largest_observed_ + 1' since that's the first packet for which
    // entropy is unknown.  If there are gaps, packets_entropy_ will
    // contain entries for all received packets with sequence_number >
    // first_gap_.
    QuicPacketSequenceNumber first_gap_;

    // Sequence number of the largest observed packet.
    QuicPacketSequenceNumber largest_observed_;

    DISALLOW_COPY_AND_ASSIGN(EntropyTracker);
  };

  explicit QuicReceivedPacketManager(CongestionFeedbackType congestion_type,
                                     QuicConnectionStats* stats);
  virtual ~QuicReceivedPacketManager();

  // Updates the internal state concerning which packets have been received.
  // bytes: the packet size in bytes including Quic Headers.
  // header: the packet header.
  // timestamp: the arrival time of the packet.
  void RecordPacketReceived(QuicByteCount bytes,
                            const QuicPacketHeader& header,
                            QuicTime receipt_time);

  void RecordPacketRevived(QuicPacketSequenceNumber sequence_number);

  // Checks whether |sequence_number| is missing and less than largest observed.
  bool IsMissing(QuicPacketSequenceNumber sequence_number);

  // Checks if we're still waiting for the packet with |sequence_number|.
  bool IsAwaitingPacket(QuicPacketSequenceNumber sequence_number);

  // Update the |received_info| for an outgoing ack.
  void UpdateReceivedPacketInfo(ReceivedPacketInfo* received_info,
                                QuicTime approximate_now);

  // Should be called before sending an ACK packet, to decide if we need
  // to attach a QuicCongestionFeedbackFrame block.
  // Returns false if no QuicCongestionFeedbackFrame block is needed.
  // Otherwise fills in feedback and returns true.
  virtual bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* feedback);

  // QuicReceivedEntropyHashCalculatorInterface
  // Called by QuicFramer, when the outgoing ack gets truncated, to recalculate
  // the received entropy hash for the truncated ack frame.
  virtual QuicPacketEntropyHash EntropyHash(
      QuicPacketSequenceNumber sequence_number) const OVERRIDE;

  // Updates internal state based on |received_info|.
  void UpdatePacketInformationReceivedByPeer(
      const ReceivedPacketInfo& received_nfo);
  // Updates internal state based on |stop_waiting|.
  void UpdatePacketInformationSentByPeer(
      const QuicStopWaitingFrame& stop_waiting);

  // Returns whether the peer is missing packets.
  bool HasMissingPackets();

  // Returns true when there are new missing packets to be reported within 3
  // packets of the largest observed.
  bool HasNewMissingPackets();

  QuicPacketSequenceNumber peer_largest_observed_packet() {
    return peer_largest_observed_packet_;
  }

  QuicPacketSequenceNumber least_packet_awaited_by_peer() {
    return least_packet_awaited_by_peer_;
  }

  QuicPacketSequenceNumber peer_least_packet_awaiting_ack() {
    return peer_least_packet_awaiting_ack_;
  }

 private:
  friend class test::QuicConnectionPeer;
  friend class test::QuicReceivedPacketManagerPeer;

  // Deletes all missing packets before least unacked. The connection won't
  // process any packets with sequence number before |least_unacked| that it
  // received after this call. Returns true if there were missing packets before
  // |least_unacked| unacked, false otherwise.
  bool DontWaitForPacketsBefore(QuicPacketSequenceNumber least_unacked);

  // Tracks entropy hashes of received packets.
  EntropyTracker entropy_tracker_;

  // Track some peer state so we can do less bookkeeping.
  // Largest sequence number that the peer has observed. Mostly received,
  // missing in case of truncated acks.
  QuicPacketSequenceNumber peer_largest_observed_packet_;
  // Least sequence number which the peer is still waiting for.
  QuicPacketSequenceNumber least_packet_awaited_by_peer_;
  // Least sequence number of the the packet sent by the peer for which it
  // hasn't received an ack.
  QuicPacketSequenceNumber peer_least_packet_awaiting_ack_;

  // Received packet information used to produce acks.
  ReceivedPacketInfo received_info_;

  // The time we received the largest_observed sequence number, or zero if
  // no sequence numbers have been received since UpdateReceivedPacketInfo.
  // Needed for calculating delta_time_largest_observed.
  QuicTime time_largest_observed_;

  scoped_ptr<ReceiveAlgorithmInterface> receive_algorithm_;

  QuicConnectionStats* stats_;

  DISALLOW_COPY_AND_ASSIGN(QuicReceivedPacketManager);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_RECEIVED_PACKET_MANAGER_H_
