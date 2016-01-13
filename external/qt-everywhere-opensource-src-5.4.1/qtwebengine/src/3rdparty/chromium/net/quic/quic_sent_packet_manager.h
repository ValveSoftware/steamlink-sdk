// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SENT_PACKET_MANAGER_H_
#define NET_QUIC_QUIC_SENT_PACKET_MANAGER_H_

#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/linked_hash_map.h"
#include "net/quic/congestion_control/loss_detection_interface.h"
#include "net/quic/congestion_control/rtt_stats.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_ack_notifier_manager.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_unacked_packet_map.h"

namespace net {

namespace test {
class QuicConnectionPeer;
class QuicSentPacketManagerPeer;
}  // namespace test

class QuicClock;
class QuicConfig;
struct QuicConnectionStats;

// Class which tracks the set of packets sent on a QUIC connection and contains
// a send algorithm to decide when to send new packets.  It keeps track of any
// retransmittable data associated with each packet. If a packet is
// retransmitted, it will keep track of each version of a packet so that if a
// previous transmission is acked, the data will not be retransmitted.
class NET_EXPORT_PRIVATE QuicSentPacketManager {
 public:
  // Interface which gets callbacks from the QuicSentPacketManager at
  // interesting points.  Implementations must not mutate the state of
  // the packet manager or connection as a result of these callbacks.
  class NET_EXPORT_PRIVATE DebugDelegate {
   public:
    virtual ~DebugDelegate() {}

    // Called when a spurious retransmission is detected.
    virtual void OnSpuriousPacketRetransmition(
        TransmissionType transmission_type,
        QuicByteCount byte_size) {}
  };

  // Struct to store the pending retransmission information.
  struct PendingRetransmission {
    PendingRetransmission(QuicPacketSequenceNumber sequence_number,
                          TransmissionType transmission_type,
                          const RetransmittableFrames& retransmittable_frames,
                          QuicSequenceNumberLength sequence_number_length)
            : sequence_number(sequence_number),
              transmission_type(transmission_type),
              retransmittable_frames(retransmittable_frames),
              sequence_number_length(sequence_number_length) {
        }

        QuicPacketSequenceNumber sequence_number;
        TransmissionType transmission_type;
        const RetransmittableFrames& retransmittable_frames;
        QuicSequenceNumberLength sequence_number_length;
  };

  QuicSentPacketManager(bool is_server,
                        const QuicClock* clock,
                        QuicConnectionStats* stats,
                        CongestionFeedbackType congestion_type,
                        LossDetectionType loss_type);
  virtual ~QuicSentPacketManager();

  virtual void SetFromConfig(const QuicConfig& config);

  // Called when a new packet is serialized.  If the packet contains
  // retransmittable data, it will be added to the unacked packet map.
  void OnSerializedPacket(const SerializedPacket& serialized_packet);

  // Called when a packet is retransmitted with a new sequence number.
  // Replaces the old entry in the unacked packet map with the new
  // sequence number.
  void OnRetransmittedPacket(QuicPacketSequenceNumber old_sequence_number,
                             QuicPacketSequenceNumber new_sequence_number);

  // Processes the incoming ack.
  void OnIncomingAck(const ReceivedPacketInfo& received_info,
                     QuicTime ack_receive_time);

  // Returns true if the non-FEC packet |sequence_number| is unacked.
  bool IsUnacked(QuicPacketSequenceNumber sequence_number) const;

  // Requests retransmission of all unacked packets of |retransmission_type|.
  void RetransmitUnackedPackets(RetransmissionType retransmission_type);

  // Retransmits the oldest pending packet there is still a tail loss probe
  // pending.  Invoked after OnRetransmissionTimeout.
  bool MaybeRetransmitTailLossProbe();

  // Removes the retransmittable frames from all unencrypted packets to ensure
  // they don't get retransmitted.
  void NeuterUnencryptedPackets();

  // Returns true if the unacked packet |sequence_number| has retransmittable
  // frames.  This will only return false if the packet has been acked, if a
  // previous transmission of this packet was ACK'd, or if this packet has been
  // retransmitted as with different sequence number.
  bool HasRetransmittableFrames(QuicPacketSequenceNumber sequence_number) const;

  // Returns true if there are pending retransmissions.
  bool HasPendingRetransmissions() const;

  // Retrieves the next pending retransmission.
  PendingRetransmission NextPendingRetransmission();

  bool HasUnackedPackets() const;

  // Returns the smallest sequence number of a serialized packet which has not
  // been acked by the peer.  If there are no unacked packets, returns 0.
  QuicPacketSequenceNumber GetLeastUnackedSentPacket() const;

  // Called when a congestion feedback frame is received from peer.
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame,
      const QuicTime& feedback_receive_time);

  // Called when we have sent bytes to the peer.  This informs the manager both
  // the number of bytes sent and if they were retransmitted.  Returns true if
  // the sender should reset the retransmission timer.
  virtual bool OnPacketSent(QuicPacketSequenceNumber sequence_number,
                            QuicTime sent_time,
                            QuicByteCount bytes,
                            TransmissionType transmission_type,
                            HasRetransmittableData has_retransmittable_data);

  // Called when the retransmission timer expires.
  virtual void OnRetransmissionTimeout();

  // Calculate the time until we can send the next packet to the wire.
  // Note 1: When kUnknownWaitTime is returned, there is no need to poll
  // TimeUntilSend again until we receive an OnIncomingAckFrame event.
  // Note 2: Send algorithms may or may not use |retransmit| in their
  // calculations.
  virtual QuicTime::Delta TimeUntilSend(QuicTime now,
                                        HasRetransmittableData retransmittable);

  // Returns amount of time for delayed ack timer.
  const QuicTime::Delta DelayedAckTime() const;

  // Returns the current delay for the retransmission timer, which may send
  // either a tail loss probe or do a full RTO.  Returns QuicTime::Zero() if
  // there are no retransmittable packets.
  const QuicTime GetRetransmissionTime() const;

  const RttStats* GetRttStats() const;

  // Returns the estimated bandwidth calculated by the congestion algorithm.
  QuicBandwidth BandwidthEstimate() const;

  // Returns the size of the current congestion window in bytes.  Note, this is
  // not the *available* window.  Some send algorithms may not use a congestion
  // window and will return 0.
  QuicByteCount GetCongestionWindow() const;

  // Enables pacing if it has not already been enabled, and if
  // FLAGS_enable_quic_pacing is set.
  void MaybeEnablePacing();

  bool using_pacing() const { return using_pacing_; }

  void set_debug_delegate(DebugDelegate* debug_delegate) {
    debug_delegate_ = debug_delegate;
  }

 private:
  friend class test::QuicConnectionPeer;
  friend class test::QuicSentPacketManagerPeer;

  // The retransmission timer is a single timer which switches modes depending
  // upon connection state.
  enum RetransmissionTimeoutMode {
    // A conventional TCP style RTO.
    RTO_MODE,
    // A tail loss probe.  By default, QUIC sends up to two before RTOing.
    TLP_MODE,
    // Retransmission of handshake packets prior to handshake completion.
    HANDSHAKE_MODE,
    // Re-invoke the loss detection when a packet is not acked before the
    // loss detection algorithm expects.
    LOSS_MODE,
  };

  typedef linked_hash_map<QuicPacketSequenceNumber,
                          TransmissionType> PendingRetransmissionMap;

  // Process the incoming ack looking for newly ack'd data packets.
  void HandleAckForSentPackets(const ReceivedPacketInfo& received_info);

  // Returns the current retransmission mode.
  RetransmissionTimeoutMode GetRetransmissionMode() const;

  // Retransmits all crypto stream packets.
  void RetransmitCryptoPackets();

  // Retransmits all the packets and abandons by invoking a full RTO.
  void RetransmitAllPackets();

  // Returns the timer for retransmitting crypto handshake packets.
  const QuicTime::Delta GetCryptoRetransmissionDelay() const;

  // Returns the timer for a new tail loss probe.
  const QuicTime::Delta GetTailLossProbeDelay() const;

  // Returns the retransmission timeout, after which a full RTO occurs.
  const QuicTime::Delta GetRetransmissionDelay() const;

  // Update the RTT if the ack is for the largest acked sequence number.
  // Returns true if the rtt was updated.
  bool MaybeUpdateRTT(const ReceivedPacketInfo& received_info,
                      const QuicTime& ack_receive_time);

  // Invokes the loss detection algorithm and loses and retransmits packets if
  // necessary.
  void InvokeLossDetection(QuicTime time);

  // Invokes OnCongestionEvent if |rtt_updated| is true, there are pending acks,
  // or pending losses.  Clears pending acks and pending losses afterwards.
  // |bytes_in_flight| is the number of bytes in flight before the losses or
  // acks.
  void MaybeInvokeCongestionEvent(bool rtt_updated,
                                  QuicByteCount bytes_in_flight);

  // Marks |sequence_number| as having been revived by the peer, but not
  // received, so the packet remains pending if it is and the congestion control
  // does not consider the packet acked.
  void MarkPacketRevived(QuicPacketSequenceNumber sequence_number,
                         QuicTime::Delta delta_largest_observed);

  // Removes the retransmittability and pending properties from the packet at
  // |it| due to receipt by the peer.  Returns an iterator to the next remaining
  // unacked packet.
  QuicUnackedPacketMap::const_iterator MarkPacketHandled(
      QuicUnackedPacketMap::const_iterator it,
      QuicTime::Delta delta_largest_observed);

  // Request that |sequence_number| be retransmitted after the other pending
  // retransmissions.  Does not add it to the retransmissions if it's already
  // a pending retransmission.
  void MarkForRetransmission(QuicPacketSequenceNumber sequence_number,
                             TransmissionType transmission_type);

  // Notify observers about spurious retransmits.
  void RecordSpuriousRetransmissions(
      const SequenceNumberSet& all_transmissions,
      QuicPacketSequenceNumber acked_sequence_number);

  // Newly serialized retransmittable and fec packets are added to this map,
  // which contains owning pointers to any contained frames.  If a packet is
  // retransmitted, this map will contain entries for both the old and the new
  // packet. The old packet's retransmittable frames entry will be NULL, while
  // the new packet's entry will contain the frames to retransmit.
  // If the old packet is acked before the new packet, then the old entry will
  // be removed from the map and the new entry's retransmittable frames will be
  // set to NULL.
  QuicUnackedPacketMap unacked_packets_;

  // Pending retransmissions which have not been packetized and sent yet.
  PendingRetransmissionMap pending_retransmissions_;

  // Tracks if the connection was created by the server.
  bool is_server_;

  // An AckNotifier can register to be informed when ACKs have been received for
  // all packets that a given block of data was sent in. The AckNotifierManager
  // maintains the currently active notifiers.
  AckNotifierManager ack_notifier_manager_;

  const QuicClock* clock_;
  QuicConnectionStats* stats_;
  DebugDelegate* debug_delegate_;
  RttStats rtt_stats_;
  scoped_ptr<SendAlgorithmInterface> send_algorithm_;
  scoped_ptr<LossDetectionInterface> loss_algorithm_;

  QuicPacketSequenceNumber largest_observed_;  // From the most recent ACK.
  // Number of times the RTO timer has fired in a row without receiving an ack.
  size_t consecutive_rto_count_;
  // Number of times the tail loss probe has been sent.
  size_t consecutive_tlp_count_;
  // Number of times the crypto handshake has been retransmitted.
  size_t consecutive_crypto_retransmission_count_;
  // Whether a tlp packet can be sent even if the send algorithm says not to.
  bool pending_tlp_transmission_;
  // Maximum number of tail loss probes to send before firing an RTO.
  size_t max_tail_loss_probes_;
  bool using_pacing_;

  // Sets of packets acked and lost as a result of the last congestion event.
  SendAlgorithmInterface::CongestionMap packets_acked_;
  SendAlgorithmInterface::CongestionMap packets_lost_;

  DISALLOW_COPY_AND_ASSIGN(QuicSentPacketManager);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SENT_PACKET_MANAGER_H_
