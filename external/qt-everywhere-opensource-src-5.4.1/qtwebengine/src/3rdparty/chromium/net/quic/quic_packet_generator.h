// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Responsible for generating packets on behalf of a QuicConnection.
// Packets are serialized just-in-time.  Control frames are queued.
// Ack and Feedback frames will be requested from the Connection
// just-in-time.  When a packet needs to be sent, the Generator
// will serialize a packet and pass it to QuicConnection::SendOrQueuePacket()
//
// The Generator's mode of operation is controlled by two conditions:
//
// 1) Is the Delegate writable?
//
// If the Delegate is not writable, then no operations will cause
// a packet to be serialized.  In particular:
// * SetShouldSendAck will simply record that an ack is to be sent.
// * AddControlFrame will enqueue the control frame.
// * ConsumeData will do nothing.
//
// If the Delegate is writable, then the behavior depends on the second
// condition:
//
// 2) Is the Generator in batch mode?
//
// If the Generator is NOT in batch mode, then each call to a write
// operation will serialize one or more packets.  The contents will
// include any previous queued frames.  If an ack should be sent
// but has not been sent, then the Delegate will be asked to create
// an Ack frame which will then be included in the packet.  When
// the write call completes, the current packet will be serialized
// and sent to the Delegate, even if it is not full.
//
// If the Generator is in batch mode, then each write operation will
// add data to the "current" packet.  When the current packet becomes
// full, it will be serialized and sent to the packet.  When batch
// mode is ended via |FinishBatchOperations|, the current packet
// will be serialzied, even if it is not full.
//
// FEC behavior also depends on batch mode.  In batch mode, FEC packets
// will be sent after |max_packets_per_group| have been sent, as well
// as after batch operations are complete.  When not in batch mode,
// an FEC packet will be sent after each write call completes.
//
// TODO(rch): This behavior should probably be tuned.  When not in batch
// mode, we should probably set a timer so that several independent
// operations can be grouped into the same FEC group.
//
// When an FEC packet is generated, it will be send to the Delegate,
// even if the Delegate has become unwritable after handling the
// data packet immediately proceeding the FEC packet.

#ifndef NET_QUIC_QUIC_PACKET_GENERATOR_H_
#define NET_QUIC_QUIC_PACKET_GENERATOR_H_

#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_types.h"

namespace net {

namespace test {
class QuicPacketGeneratorPeer;
}  // namespace test

class QuicAckNotifier;

class NET_EXPORT_PRIVATE QuicPacketGenerator {
 public:
  class NET_EXPORT_PRIVATE DelegateInterface {
   public:
    virtual ~DelegateInterface() {}
    virtual bool ShouldGeneratePacket(TransmissionType transmission_type,
                                      HasRetransmittableData retransmittable,
                                      IsHandshake handshake) = 0;
    virtual QuicAckFrame* CreateAckFrame() = 0;
    virtual QuicCongestionFeedbackFrame* CreateFeedbackFrame() = 0;
    virtual QuicStopWaitingFrame* CreateStopWaitingFrame() = 0;
    // Takes ownership of |packet.packet| and |packet.retransmittable_frames|.
    virtual bool OnSerializedPacket(const SerializedPacket& packet) = 0;
    virtual void CloseConnection(QuicErrorCode error, bool from_peer) = 0;
  };

  // Interface which gets callbacks from the QuicPacketGenerator at interesting
  // points.  Implementations must not mutate the state of the generator
  // as a result of these callbacks.
  class NET_EXPORT_PRIVATE DebugDelegate {
   public:
    virtual ~DebugDelegate() {}

    // Called when a frame has been added to the current packet.
    virtual void OnFrameAddedToPacket(const QuicFrame& frame) {}
  };

  QuicPacketGenerator(QuicConnectionId connection_id,
                      QuicFramer* framer,
                      QuicRandom* random_generator,
                      DelegateInterface* delegate);

  virtual ~QuicPacketGenerator();

  // Indicates that an ACK frame should be sent.  If |also_send_feedback| is
  // true, then it also indicates a CONGESTION_FEEDBACK frame should be sent.
  // If |also_send_stop_waiting| is true, then it also indicates that a
  // STOP_WAITING frame should be sent as well.
  // The contents of the frame(s) will be generated via a call to the delegates
  // CreateAckFrame() and CreateFeedbackFrame() when the packet is serialized.
  void SetShouldSendAck(bool also_send_feedback,
                        bool also_send_stop_waiting);

  // Indicates that a STOP_WAITING frame should be sent.
  void SetShouldSendStopWaiting();

  void AddControlFrame(const QuicFrame& frame);

  // Given some data, may consume part or all of it and pass it to the
  // packet creator to be serialized into packets. If not in batch
  // mode, these packets will also be sent during this call. Also
  // attaches a QuicAckNotifier to any created stream frames, which
  // will be called once the frame is ACKed by the peer. The
  // QuicAckNotifier is owned by the QuicConnection. |notifier| may
  // be NULL.
  QuicConsumedData ConsumeData(QuicStreamId id,
                               const IOVector& data,
                               QuicStreamOffset offset,
                               bool fin,
                               FecProtection fec_protection,
                               QuicAckNotifier* notifier);

  // Indicates whether batch mode is currently enabled.
  bool InBatchMode();
  // Disables flushing.
  void StartBatchOperations();
  // Enables flushing and flushes queued data which can be sent.
  void FinishBatchOperations();

  // Flushes all queued frames, even frames which are not sendable.
  void FlushAllQueuedFrames();

  bool HasQueuedFrames() const;

  // Makes the framer not serialize the protocol version in sent packets.
  void StopSendingVersion();

  // Creates a version negotiation packet which supports |supported_versions|.
  // Caller owns the created  packet. Also, sets the entropy hash of the
  // serialized packet to a random bool and returns that value as a member of
  // SerializedPacket.
  QuicEncryptedPacket* SerializeVersionNegotiationPacket(
      const QuicVersionVector& supported_versions);


  // Re-serializes frames with the original packet's sequence number length.
  // Used for retransmitting packets to ensure they aren't too long.
  // Caller must ensure that any open FEC group is closed before calling this
  // method.
  SerializedPacket ReserializeAllFrames(
      const QuicFrames& frames,
      QuicSequenceNumberLength original_length);

  // Update the sequence number length to use in future packets as soon as it
  // can be safely changed.
  void UpdateSequenceNumberLength(
      QuicPacketSequenceNumber least_packet_awaited_by_peer,
      QuicByteCount congestion_window);

  // Sets the encryption level that will be applied to new packets.
  void set_encryption_level(EncryptionLevel level);

  // Sequence number of the last created packet, or 0 if no packets have been
  // created.
  QuicPacketSequenceNumber sequence_number() const;

  size_t max_packet_length() const;

  void set_max_packet_length(size_t length);

  void set_debug_delegate(DebugDelegate* debug_delegate) {
    debug_delegate_ = debug_delegate;
  }

 private:
  friend class test::QuicPacketGeneratorPeer;

  // Turn on FEC protection for subsequent packets in the generator.
  // If no FEC group is currently open in the creator, this method flushes any
  // queued frames in the generator and in the creator, and it then turns FEC on
  // in the creator. This method may be called with an open FEC group in the
  // creator, in which case, only the generator's state is altered.
  void MaybeStartFecProtection();

  // Serializes and calls the delegate on an FEC packet if one was under
  // construction in the creator. When |force| is false, it relies on the
  // creator being ready to send an FEC packet, otherwise FEC packet is sent
  // as long as one is under construction in the creator.  Also tries to turns
  // off FEC protection in the creator if it's off in the generator.
  void MaybeSendFecPacketAndCloseGroup(bool force);

  void SendQueuedFrames(bool flush);

  // Test to see if we have pending ack, feedback, or control frames.
  bool HasPendingFrames() const;
  // Test to see if the addition of a pending frame (which might be
  // retransmittable) would still allow the resulting packet to be sent now.
  bool CanSendWithNextPendingFrameAddition() const;
  // Add exactly one pending frame, preferring ack over feedback over control
  // frames.
  bool AddNextPendingFrame();

  bool AddFrame(const QuicFrame& frame);

  void SerializeAndSendPacket();

  DelegateInterface* delegate_;
  DebugDelegate* debug_delegate_;

  QuicPacketCreator packet_creator_;
  QuicFrames queued_control_frames_;

  // True if batch mode is currently enabled.
  bool batch_mode_;

  // True if FEC protection is on. The creator may have an open FEC group even
  // if this variable is false.
  bool should_fec_protect_;

  // Flags to indicate the need for just-in-time construction of a frame.
  bool should_send_ack_;
  bool should_send_feedback_;
  bool should_send_stop_waiting_;
  // If we put a non-retransmittable frame (namley ack or feedback frame) in
  // this packet, then we have to hold a reference to it until we flush (and
  // serialize it). Retransmittable frames are referenced elsewhere so that they
  // can later be (optionally) retransmitted.
  scoped_ptr<QuicAckFrame> pending_ack_frame_;
  scoped_ptr<QuicCongestionFeedbackFrame> pending_feedback_frame_;
  scoped_ptr<QuicStopWaitingFrame> pending_stop_waiting_frame_;

  DISALLOW_COPY_AND_ASSIGN(QuicPacketGenerator);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PACKET_GENERATOR_H_
