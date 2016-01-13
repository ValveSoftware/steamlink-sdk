// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection.h"

#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "net/base/net_errors.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/iovector.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using base::hash_map;
using base::hash_set;
using std::list;
using std::make_pair;
using std::max;
using std::min;
using std::numeric_limits;
using std::set;
using std::string;
using std::vector;

namespace net {

class QuicDecrypter;
class QuicEncrypter;

namespace {

// The largest gap in packets we'll accept without closing the connection.
// This will likely have to be tuned.
const QuicPacketSequenceNumber kMaxPacketGap = 5000;

// Limit the number of FEC groups to two.  If we get enough out of order packets
// that this becomes limiting, we can revisit.
const size_t kMaxFecGroups = 2;

// Limit the number of undecryptable packets we buffer in
// expectation of the CHLO/SHLO arriving.
const size_t kMaxUndecryptablePackets = 10;

bool Near(QuicPacketSequenceNumber a, QuicPacketSequenceNumber b) {
  QuicPacketSequenceNumber delta = (a > b) ? a - b : b - a;
  return delta <= kMaxPacketGap;
}

// An alarm that is scheduled to send an ack if a timeout occurs.
class AckAlarm : public QuicAlarm::Delegate {
 public:
  explicit AckAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual QuicTime OnAlarm() OVERRIDE {
    connection_->SendAck();
    return QuicTime::Zero();
  }

 private:
  QuicConnection* connection_;

  DISALLOW_COPY_AND_ASSIGN(AckAlarm);
};

// This alarm will be scheduled any time a data-bearing packet is sent out.
// When the alarm goes off, the connection checks to see if the oldest packets
// have been acked, and retransmit them if they have not.
class RetransmissionAlarm : public QuicAlarm::Delegate {
 public:
  explicit RetransmissionAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual QuicTime OnAlarm() OVERRIDE {
    connection_->OnRetransmissionTimeout();
    return QuicTime::Zero();
  }

 private:
  QuicConnection* connection_;

  DISALLOW_COPY_AND_ASSIGN(RetransmissionAlarm);
};

// An alarm that is scheduled when the sent scheduler requires a
// a delay before sending packets and fires when the packet may be sent.
class SendAlarm : public QuicAlarm::Delegate {
 public:
  explicit SendAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual QuicTime OnAlarm() OVERRIDE {
    connection_->WriteIfNotBlocked();
    // Never reschedule the alarm, since CanWrite does that.
    return QuicTime::Zero();
  }

 private:
  QuicConnection* connection_;

  DISALLOW_COPY_AND_ASSIGN(SendAlarm);
};

class TimeoutAlarm : public QuicAlarm::Delegate {
 public:
  explicit TimeoutAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual QuicTime OnAlarm() OVERRIDE {
    connection_->CheckForTimeout();
    // Never reschedule the alarm, since CheckForTimeout does that.
    return QuicTime::Zero();
  }

 private:
  QuicConnection* connection_;

  DISALLOW_COPY_AND_ASSIGN(TimeoutAlarm);
};

class PingAlarm : public QuicAlarm::Delegate {
 public:
  explicit PingAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual QuicTime OnAlarm() OVERRIDE {
    connection_->SendPing();
    return QuicTime::Zero();
  }

 private:
  QuicConnection* connection_;

  DISALLOW_COPY_AND_ASSIGN(PingAlarm);
};

QuicConnection::PacketType GetPacketType(
    const RetransmittableFrames* retransmittable_frames) {
  if (!retransmittable_frames) {
    return QuicConnection::NORMAL;
  }
  for (size_t i = 0; i < retransmittable_frames->frames().size(); ++i) {
    if (retransmittable_frames->frames()[i].type == CONNECTION_CLOSE_FRAME) {
      return QuicConnection::CONNECTION_CLOSE;
    }
  }
  return QuicConnection::NORMAL;
}

}  // namespace

QuicConnection::QueuedPacket::QueuedPacket(SerializedPacket packet,
                                           EncryptionLevel level,
                                           TransmissionType transmission_type)
  : sequence_number(packet.sequence_number),
    packet(packet.packet),
    encryption_level(level),
    transmission_type(transmission_type),
    retransmittable((transmission_type != NOT_RETRANSMISSION ||
                     packet.retransmittable_frames != NULL) ?
                         HAS_RETRANSMITTABLE_DATA : NO_RETRANSMITTABLE_DATA),
    handshake(packet.retransmittable_frames == NULL ?
      NOT_HANDSHAKE : packet.retransmittable_frames->HasCryptoHandshake()),
    type(GetPacketType(packet.retransmittable_frames)),
    length(packet.packet->length()) {
}

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")

QuicConnection::QuicConnection(QuicConnectionId connection_id,
                               IPEndPoint address,
                               QuicConnectionHelperInterface* helper,
                               QuicPacketWriter* writer,
                               bool is_server,
                               const QuicVersionVector& supported_versions)
    : framer_(supported_versions, helper->GetClock()->ApproximateNow(),
              is_server),
      helper_(helper),
      writer_(writer),
      encryption_level_(ENCRYPTION_NONE),
      clock_(helper->GetClock()),
      random_generator_(helper->GetRandomGenerator()),
      connection_id_(connection_id),
      peer_address_(address),
      migrating_peer_port_(0),
      last_packet_revived_(false),
      last_size_(0),
      last_decrypted_packet_level_(ENCRYPTION_NONE),
      largest_seen_packet_with_ack_(0),
      largest_seen_packet_with_stop_waiting_(0),
      pending_version_negotiation_packet_(false),
      received_packet_manager_(kTCP, &stats_),
      ack_queued_(false),
      stop_waiting_count_(0),
      ack_alarm_(helper->CreateAlarm(new AckAlarm(this))),
      retransmission_alarm_(helper->CreateAlarm(new RetransmissionAlarm(this))),
      send_alarm_(helper->CreateAlarm(new SendAlarm(this))),
      resume_writes_alarm_(helper->CreateAlarm(new SendAlarm(this))),
      timeout_alarm_(helper->CreateAlarm(new TimeoutAlarm(this))),
      ping_alarm_(helper->CreateAlarm(new PingAlarm(this))),
      debug_visitor_(NULL),
      packet_generator_(connection_id_, &framer_, random_generator_, this),
      idle_network_timeout_(
          QuicTime::Delta::FromSeconds(kDefaultInitialTimeoutSecs)),
      overall_connection_timeout_(QuicTime::Delta::Infinite()),
      time_of_last_received_packet_(clock_->ApproximateNow()),
      time_of_last_sent_new_packet_(clock_->ApproximateNow()),
      sequence_number_of_last_sent_packet_(0),
      sent_packet_manager_(
          is_server, clock_, &stats_, kTCP,
          FLAGS_quic_use_time_loss_detection ? kTime : kNack),
      version_negotiation_state_(START_NEGOTIATION),
      is_server_(is_server),
      connected_(true),
      peer_ip_changed_(false),
      peer_port_changed_(false),
      self_ip_changed_(false),
      self_port_changed_(false) {
  if (!is_server_) {
    // Pacing will be enabled if the client negotiates it.
    sent_packet_manager_.MaybeEnablePacing();
  }
  DVLOG(1) << ENDPOINT << "Created connection with connection_id: "
           << connection_id;
  timeout_alarm_->Set(clock_->ApproximateNow().Add(idle_network_timeout_));
  framer_.set_visitor(this);
  framer_.set_received_entropy_calculator(&received_packet_manager_);
  stats_.connection_creation_time = clock_->ApproximateNow();
}

QuicConnection::~QuicConnection() {
  STLDeleteElements(&undecryptable_packets_);
  STLDeleteValues(&group_map_);
  for (QueuedPacketList::iterator it = queued_packets_.begin();
       it != queued_packets_.end(); ++it) {
    delete it->packet;
  }
}

void QuicConnection::SetFromConfig(const QuicConfig& config) {
  SetIdleNetworkTimeout(config.idle_connection_state_lifetime());
  sent_packet_manager_.SetFromConfig(config);
  // TODO(satyamshekhar): Set congestion control and ICSL also.
}

bool QuicConnection::SelectMutualVersion(
    const QuicVersionVector& available_versions) {
  // Try to find the highest mutual version by iterating over supported
  // versions, starting with the highest, and breaking out of the loop once we
  // find a matching version in the provided available_versions vector.
  const QuicVersionVector& supported_versions = framer_.supported_versions();
  for (size_t i = 0; i < supported_versions.size(); ++i) {
    const QuicVersion& version = supported_versions[i];
    if (std::find(available_versions.begin(), available_versions.end(),
                  version) != available_versions.end()) {
      framer_.set_version(version);
      return true;
    }
  }

  return false;
}

void QuicConnection::OnError(QuicFramer* framer) {
  // Packets that we cannot decrypt are dropped.
  // TODO(rch): add stats to measure this.
  if (!connected_ || framer->error() == QUIC_DECRYPTION_FAILURE) {
    return;
  }
  SendConnectionCloseWithDetails(framer->error(), framer->detailed_error());
}

void QuicConnection::OnPacket() {
  DCHECK(last_stream_frames_.empty() &&
         last_goaway_frames_.empty() &&
         last_window_update_frames_.empty() &&
         last_blocked_frames_.empty() &&
         last_rst_frames_.empty() &&
         last_ack_frames_.empty() &&
         last_congestion_frames_.empty() &&
         last_stop_waiting_frames_.empty());
}

void QuicConnection::OnPublicResetPacket(
    const QuicPublicResetPacket& packet) {
  if (debug_visitor_) {
    debug_visitor_->OnPublicResetPacket(packet);
  }
  CloseConnection(QUIC_PUBLIC_RESET, true);
}

bool QuicConnection::OnProtocolVersionMismatch(QuicVersion received_version) {
  DVLOG(1) << ENDPOINT << "Received packet with mismatched version "
           << received_version;
  // TODO(satyamshekhar): Implement no server state in this mode.
  if (!is_server_) {
    LOG(DFATAL) << ENDPOINT << "Framer called OnProtocolVersionMismatch. "
                << "Closing connection.";
    CloseConnection(QUIC_INTERNAL_ERROR, false);
    return false;
  }
  DCHECK_NE(version(), received_version);

  if (debug_visitor_) {
    debug_visitor_->OnProtocolVersionMismatch(received_version);
  }

  switch (version_negotiation_state_) {
    case START_NEGOTIATION:
      if (!framer_.IsSupportedVersion(received_version)) {
        SendVersionNegotiationPacket();
        version_negotiation_state_ = NEGOTIATION_IN_PROGRESS;
        return false;
      }
      break;

    case NEGOTIATION_IN_PROGRESS:
      if (!framer_.IsSupportedVersion(received_version)) {
        SendVersionNegotiationPacket();
        return false;
      }
      break;

    case NEGOTIATED_VERSION:
      // Might be old packets that were sent by the client before the version
      // was negotiated. Drop these.
      return false;

    default:
      DCHECK(false);
  }

  version_negotiation_state_ = NEGOTIATED_VERSION;
  visitor_->OnSuccessfulVersionNegotiation(received_version);
  DVLOG(1) << ENDPOINT << "version negotiated " << received_version;

  // Store the new version.
  framer_.set_version(received_version);

  // TODO(satyamshekhar): Store the sequence number of this packet and close the
  // connection if we ever received a packet with incorrect version and whose
  // sequence number is greater.
  return true;
}

// Handles version negotiation for client connection.
void QuicConnection::OnVersionNegotiationPacket(
    const QuicVersionNegotiationPacket& packet) {
  if (is_server_) {
    LOG(DFATAL) << ENDPOINT << "Framer parsed VersionNegotiationPacket."
                << " Closing connection.";
    CloseConnection(QUIC_INTERNAL_ERROR, false);
    return;
  }
  if (debug_visitor_) {
    debug_visitor_->OnVersionNegotiationPacket(packet);
  }

  if (version_negotiation_state_ != START_NEGOTIATION) {
    // Possibly a duplicate version negotiation packet.
    return;
  }

  if (std::find(packet.versions.begin(),
                packet.versions.end(), version()) !=
      packet.versions.end()) {
    DLOG(WARNING) << ENDPOINT << "The server already supports our version. "
                  << "It should have accepted our connection.";
    // Just drop the connection.
    CloseConnection(QUIC_INVALID_VERSION_NEGOTIATION_PACKET, false);
    return;
  }

  if (!SelectMutualVersion(packet.versions)) {
    SendConnectionCloseWithDetails(QUIC_INVALID_VERSION,
                                   "no common version found");
    return;
  }

  DVLOG(1) << ENDPOINT << "negotiating version " << version();
  server_supported_versions_ = packet.versions;
  version_negotiation_state_ = NEGOTIATION_IN_PROGRESS;
  RetransmitUnackedPackets(ALL_PACKETS);
}

void QuicConnection::OnRevivedPacket() {
}

bool QuicConnection::OnUnauthenticatedPublicHeader(
    const QuicPacketPublicHeader& header) {
  return true;
}

bool QuicConnection::OnUnauthenticatedHeader(const QuicPacketHeader& header) {
  return true;
}

void QuicConnection::OnDecryptedPacket(EncryptionLevel level) {
  last_decrypted_packet_level_ = level;
}

bool QuicConnection::OnPacketHeader(const QuicPacketHeader& header) {
  if (debug_visitor_) {
    debug_visitor_->OnPacketHeader(header);
  }

  if (!ProcessValidatedPacket()) {
    return false;
  }

  // Will be decrement below if we fall through to return true;
  ++stats_.packets_dropped;

  if (header.public_header.connection_id != connection_id_) {
    DVLOG(1) << ENDPOINT << "Ignoring packet from unexpected ConnectionId: "
             << header.public_header.connection_id << " instead of "
             << connection_id_;
    return false;
  }

  if (!Near(header.packet_sequence_number,
            last_header_.packet_sequence_number)) {
    DVLOG(1) << ENDPOINT << "Packet " << header.packet_sequence_number
             << " out of bounds.  Discarding";
    SendConnectionCloseWithDetails(QUIC_INVALID_PACKET_HEADER,
                                   "Packet sequence number out of bounds");
    return false;
  }

  // If this packet has already been seen, or that the sender
  // has told us will not be retransmitted, then stop processing the packet.
  if (!received_packet_manager_.IsAwaitingPacket(
          header.packet_sequence_number)) {
    DVLOG(1) << ENDPOINT << "Packet " << header.packet_sequence_number
             << " no longer being waited for.  Discarding.";
    // TODO(jri): Log reception of duplicate packets or packets the peer has
    // told us to stop waiting for.
    return false;
  }

  if (version_negotiation_state_ != NEGOTIATED_VERSION) {
    if (is_server_) {
      if (!header.public_header.version_flag) {
        DLOG(WARNING) << ENDPOINT << "Packet " << header.packet_sequence_number
                      << " without version flag before version negotiated.";
        // Packets should have the version flag till version negotiation is
        // done.
        CloseConnection(QUIC_INVALID_VERSION, false);
        return false;
      } else {
        DCHECK_EQ(1u, header.public_header.versions.size());
        DCHECK_EQ(header.public_header.versions[0], version());
        version_negotiation_state_ = NEGOTIATED_VERSION;
        visitor_->OnSuccessfulVersionNegotiation(version());
      }
    } else {
      DCHECK(!header.public_header.version_flag);
      // If the client gets a packet without the version flag from the server
      // it should stop sending version since the version negotiation is done.
      packet_generator_.StopSendingVersion();
      version_negotiation_state_ = NEGOTIATED_VERSION;
      visitor_->OnSuccessfulVersionNegotiation(version());
    }
  }

  DCHECK_EQ(NEGOTIATED_VERSION, version_negotiation_state_);

  --stats_.packets_dropped;
  DVLOG(1) << ENDPOINT << "Received packet header: " << header;
  last_header_ = header;
  DCHECK(connected_);
  return true;
}

void QuicConnection::OnFecProtectedPayload(StringPiece payload) {
  DCHECK_EQ(IN_FEC_GROUP, last_header_.is_in_fec_group);
  DCHECK_NE(0u, last_header_.fec_group);
  QuicFecGroup* group = GetFecGroup();
  if (group != NULL) {
    group->Update(last_decrypted_packet_level_, last_header_, payload);
  }
}

bool QuicConnection::OnStreamFrame(const QuicStreamFrame& frame) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnStreamFrame(frame);
  }
  if (frame.stream_id != kCryptoStreamId &&
      last_decrypted_packet_level_ == ENCRYPTION_NONE) {
    DLOG(WARNING) << ENDPOINT
                  << "Received an unencrypted data frame: closing connection";
    SendConnectionClose(QUIC_UNENCRYPTED_STREAM_DATA);
    return false;
  }
  last_stream_frames_.push_back(frame);
  return true;
}

bool QuicConnection::OnAckFrame(const QuicAckFrame& incoming_ack) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnAckFrame(incoming_ack);
  }
  DVLOG(1) << ENDPOINT << "OnAckFrame: " << incoming_ack;

  if (last_header_.packet_sequence_number <= largest_seen_packet_with_ack_) {
    DVLOG(1) << ENDPOINT << "Received an old ack frame: ignoring";
    return true;
  }

  if (!ValidateAckFrame(incoming_ack)) {
    SendConnectionClose(QUIC_INVALID_ACK_DATA);
    return false;
  }

  last_ack_frames_.push_back(incoming_ack);
  return connected_;
}

void QuicConnection::ProcessAckFrame(const QuicAckFrame& incoming_ack) {
  largest_seen_packet_with_ack_ = last_header_.packet_sequence_number;
  received_packet_manager_.UpdatePacketInformationReceivedByPeer(
      incoming_ack.received_info);
  if (version() <= QUIC_VERSION_15) {
    ProcessStopWaitingFrame(incoming_ack.sent_info);
  }

  sent_entropy_manager_.ClearEntropyBefore(
      received_packet_manager_.least_packet_awaited_by_peer() - 1);

  sent_packet_manager_.OnIncomingAck(incoming_ack.received_info,
                                     time_of_last_received_packet_);
  if (sent_packet_manager_.HasPendingRetransmissions()) {
    WriteIfNotBlocked();
  }

  // Always reset the retransmission alarm when an ack comes in, since we now
  // have a better estimate of the current rtt than when it was set.
  retransmission_alarm_->Cancel();
  QuicTime retransmission_time =
      sent_packet_manager_.GetRetransmissionTime();
  if (retransmission_time != QuicTime::Zero()) {
    retransmission_alarm_->Set(retransmission_time);
  }
}

void QuicConnection::ProcessStopWaitingFrame(
    const QuicStopWaitingFrame& stop_waiting) {
  largest_seen_packet_with_stop_waiting_ = last_header_.packet_sequence_number;
  received_packet_manager_.UpdatePacketInformationSentByPeer(stop_waiting);
  // Possibly close any FecGroups which are now irrelevant.
  CloseFecGroupsBefore(stop_waiting.least_unacked + 1);
}

bool QuicConnection::OnCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& feedback) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnCongestionFeedbackFrame(feedback);
  }
  last_congestion_frames_.push_back(feedback);
  return connected_;
}

bool QuicConnection::OnStopWaitingFrame(const QuicStopWaitingFrame& frame) {
  DCHECK(connected_);

  if (last_header_.packet_sequence_number <=
      largest_seen_packet_with_stop_waiting_) {
    DVLOG(1) << ENDPOINT << "Received an old stop waiting frame: ignoring";
    return true;
  }

  if (!ValidateStopWaitingFrame(frame)) {
    SendConnectionClose(QUIC_INVALID_STOP_WAITING_DATA);
    return false;
  }

  if (debug_visitor_) {
    debug_visitor_->OnStopWaitingFrame(frame);
  }

  last_stop_waiting_frames_.push_back(frame);
  return connected_;
}

bool QuicConnection::OnPingFrame(const QuicPingFrame& frame) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnPingFrame(frame);
  }
  return true;
}

bool QuicConnection::ValidateAckFrame(const QuicAckFrame& incoming_ack) {
  if (incoming_ack.received_info.largest_observed >
      packet_generator_.sequence_number()) {
    DLOG(ERROR) << ENDPOINT << "Peer's observed unsent packet:"
                << incoming_ack.received_info.largest_observed << " vs "
                << packet_generator_.sequence_number();
    // We got an error for data we have not sent.  Error out.
    return false;
  }

  if (incoming_ack.received_info.largest_observed <
          received_packet_manager_.peer_largest_observed_packet()) {
    DLOG(ERROR) << ENDPOINT << "Peer's largest_observed packet decreased:"
                << incoming_ack.received_info.largest_observed << " vs "
                << received_packet_manager_.peer_largest_observed_packet();
    // A new ack has a diminished largest_observed value.  Error out.
    // If this was an old packet, we wouldn't even have checked.
    return false;
  }

  if (version() <= QUIC_VERSION_15) {
    if (!ValidateStopWaitingFrame(incoming_ack.sent_info)) {
      return false;
    }
  }

  if (!incoming_ack.received_info.missing_packets.empty() &&
      *incoming_ack.received_info.missing_packets.rbegin() >
      incoming_ack.received_info.largest_observed) {
    DLOG(ERROR) << ENDPOINT << "Peer sent missing packet: "
                << *incoming_ack.received_info.missing_packets.rbegin()
                << " which is greater than largest observed: "
                << incoming_ack.received_info.largest_observed;
    return false;
  }

  if (!incoming_ack.received_info.missing_packets.empty() &&
      *incoming_ack.received_info.missing_packets.begin() <
      received_packet_manager_.least_packet_awaited_by_peer()) {
    DLOG(ERROR) << ENDPOINT << "Peer sent missing packet: "
                << *incoming_ack.received_info.missing_packets.begin()
                << " which is smaller than least_packet_awaited_by_peer_: "
                << received_packet_manager_.least_packet_awaited_by_peer();
    return false;
  }

  if (!sent_entropy_manager_.IsValidEntropy(
          incoming_ack.received_info.largest_observed,
          incoming_ack.received_info.missing_packets,
          incoming_ack.received_info.entropy_hash)) {
    DLOG(ERROR) << ENDPOINT << "Peer sent invalid entropy.";
    return false;
  }

  for (SequenceNumberSet::const_iterator iter =
           incoming_ack.received_info.revived_packets.begin();
       iter != incoming_ack.received_info.revived_packets.end(); ++iter) {
    if (!ContainsKey(incoming_ack.received_info.missing_packets, *iter)) {
      DLOG(ERROR) << ENDPOINT
                  << "Peer specified revived packet which was not missing.";
      return false;
    }
  }
  return true;
}

bool QuicConnection::ValidateStopWaitingFrame(
    const QuicStopWaitingFrame& stop_waiting) {
  if (stop_waiting.least_unacked <
      received_packet_manager_.peer_least_packet_awaiting_ack()) {
    DLOG(ERROR) << ENDPOINT << "Peer's sent low least_unacked: "
                << stop_waiting.least_unacked << " vs "
                << received_packet_manager_.peer_least_packet_awaiting_ack();
    // We never process old ack frames, so this number should only increase.
    return false;
  }

  if (stop_waiting.least_unacked >
      last_header_.packet_sequence_number) {
    DLOG(ERROR) << ENDPOINT << "Peer sent least_unacked:"
                << stop_waiting.least_unacked
                << " greater than the enclosing packet sequence number:"
                << last_header_.packet_sequence_number;
    return false;
  }

  return true;
}

void QuicConnection::OnFecData(const QuicFecData& fec) {
  DCHECK_EQ(IN_FEC_GROUP, last_header_.is_in_fec_group);
  DCHECK_NE(0u, last_header_.fec_group);
  QuicFecGroup* group = GetFecGroup();
  if (group != NULL) {
    group->UpdateFec(last_decrypted_packet_level_,
                     last_header_.packet_sequence_number, fec);
  }
}

bool QuicConnection::OnRstStreamFrame(const QuicRstStreamFrame& frame) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnRstStreamFrame(frame);
  }
  DVLOG(1) << ENDPOINT << "Stream reset with error "
           << QuicUtils::StreamErrorToString(frame.error_code);
  last_rst_frames_.push_back(frame);
  return connected_;
}

bool QuicConnection::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnConnectionCloseFrame(frame);
  }
  DVLOG(1) << ENDPOINT << "Connection " << connection_id()
           << " closed with error "
           << QuicUtils::ErrorToString(frame.error_code)
           << " " << frame.error_details;
  last_close_frames_.push_back(frame);
  return connected_;
}

bool QuicConnection::OnGoAwayFrame(const QuicGoAwayFrame& frame) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnGoAwayFrame(frame);
  }
  DVLOG(1) << ENDPOINT << "Go away received with error "
           << QuicUtils::ErrorToString(frame.error_code)
           << " and reason:" << frame.reason_phrase;
  last_goaway_frames_.push_back(frame);
  return connected_;
}

bool QuicConnection::OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnWindowUpdateFrame(frame);
  }
  DVLOG(1) << ENDPOINT << "WindowUpdate received for stream: "
           << frame.stream_id << " with byte offset: " << frame.byte_offset;
  last_window_update_frames_.push_back(frame);
  return connected_;
}

bool QuicConnection::OnBlockedFrame(const QuicBlockedFrame& frame) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnBlockedFrame(frame);
  }
  DVLOG(1) << ENDPOINT << "Blocked frame received for stream: "
           << frame.stream_id;
  last_blocked_frames_.push_back(frame);
  return connected_;
}

void QuicConnection::OnPacketComplete() {
  // Don't do anything if this packet closed the connection.
  if (!connected_) {
    ClearLastFrames();
    return;
  }

  DVLOG(1) << ENDPOINT << (last_packet_revived_ ? "Revived" : "Got")
           << " packet " << last_header_.packet_sequence_number
           << " with " << last_ack_frames_.size() << " acks, "
           << last_congestion_frames_.size() << " congestions, "
           << last_stop_waiting_frames_.size() << " stop_waiting, "
           << last_goaway_frames_.size() << " goaways, "
           << last_window_update_frames_.size() << " window updates, "
           << last_blocked_frames_.size() << " blocked, "
           << last_rst_frames_.size() << " rsts, "
           << last_close_frames_.size() << " closes, "
           << last_stream_frames_.size()
           << " stream frames for "
           << last_header_.public_header.connection_id;

  // Call MaybeQueueAck() before recording the received packet, since we want
  // to trigger an ack if the newly received packet was previously missing.
  MaybeQueueAck();

  // Record received or revived packet to populate ack info correctly before
  // processing stream frames, since the processing may result in a response
  // packet with a bundled ack.
  if (last_packet_revived_) {
    received_packet_manager_.RecordPacketRevived(
        last_header_.packet_sequence_number);
  } else {
    received_packet_manager_.RecordPacketReceived(
        last_size_, last_header_, time_of_last_received_packet_);
  }

  if (!last_stream_frames_.empty()) {
    visitor_->OnStreamFrames(last_stream_frames_);
  }

  for (size_t i = 0; i < last_stream_frames_.size(); ++i) {
    stats_.stream_bytes_received +=
        last_stream_frames_[i].data.TotalBufferSize();
  }

  // Process window updates, blocked, stream resets, acks, then congestion
  // feedback.
  if (!last_window_update_frames_.empty()) {
    visitor_->OnWindowUpdateFrames(last_window_update_frames_);
  }
  if (!last_blocked_frames_.empty()) {
    visitor_->OnBlockedFrames(last_blocked_frames_);
  }
  for (size_t i = 0; i < last_goaway_frames_.size(); ++i) {
    visitor_->OnGoAway(last_goaway_frames_[i]);
  }
  for (size_t i = 0; i < last_rst_frames_.size(); ++i) {
    visitor_->OnRstStream(last_rst_frames_[i]);
  }
  for (size_t i = 0; i < last_ack_frames_.size(); ++i) {
    ProcessAckFrame(last_ack_frames_[i]);
  }
  for (size_t i = 0; i < last_congestion_frames_.size(); ++i) {
    sent_packet_manager_.OnIncomingQuicCongestionFeedbackFrame(
        last_congestion_frames_[i], time_of_last_received_packet_);
  }
  for (size_t i = 0; i < last_stop_waiting_frames_.size(); ++i) {
    ProcessStopWaitingFrame(last_stop_waiting_frames_[i]);
  }
  if (!last_close_frames_.empty()) {
    CloseConnection(last_close_frames_[0].error_code, true);
    DCHECK(!connected_);
  }

  // If there are new missing packets to report, send an ack immediately.
  if (received_packet_manager_.HasNewMissingPackets()) {
    ack_queued_ = true;
    ack_alarm_->Cancel();
  }

  UpdateStopWaitingCount();

  ClearLastFrames();
}

void QuicConnection::MaybeQueueAck() {
  // If the incoming packet was missing, send an ack immediately.
  ack_queued_ = received_packet_manager_.IsMissing(
      last_header_.packet_sequence_number);

  if (!ack_queued_ && ShouldLastPacketInstigateAck()) {
    if (ack_alarm_->IsSet()) {
      ack_queued_ = true;
    } else {
      // Send an ack much more quickly for crypto handshake packets.
      QuicTime::Delta delayed_ack_time = sent_packet_manager_.DelayedAckTime();
      if (last_stream_frames_.size() == 1 &&
          last_stream_frames_[0].stream_id == kCryptoStreamId) {
        delayed_ack_time = QuicTime::Delta::Zero();
      }
      ack_alarm_->Set(clock_->ApproximateNow().Add(delayed_ack_time));
      DVLOG(1) << "Ack timer set; next packet or timer will trigger ACK.";
    }
  }

  if (ack_queued_) {
    ack_alarm_->Cancel();
  }
}

void QuicConnection::ClearLastFrames() {
  last_stream_frames_.clear();
  last_goaway_frames_.clear();
  last_window_update_frames_.clear();
  last_blocked_frames_.clear();
  last_rst_frames_.clear();
  last_ack_frames_.clear();
  last_stop_waiting_frames_.clear();
  last_congestion_frames_.clear();
}

QuicAckFrame* QuicConnection::CreateAckFrame() {
  QuicAckFrame* outgoing_ack = new QuicAckFrame();
  received_packet_manager_.UpdateReceivedPacketInfo(
      &(outgoing_ack->received_info), clock_->ApproximateNow());
  UpdateStopWaiting(&(outgoing_ack->sent_info));
  DVLOG(1) << ENDPOINT << "Creating ack frame: " << *outgoing_ack;
  return outgoing_ack;
}

QuicCongestionFeedbackFrame* QuicConnection::CreateFeedbackFrame() {
  return new QuicCongestionFeedbackFrame(outgoing_congestion_feedback_);
}

QuicStopWaitingFrame* QuicConnection::CreateStopWaitingFrame() {
  QuicStopWaitingFrame stop_waiting;
  UpdateStopWaiting(&stop_waiting);
  return new QuicStopWaitingFrame(stop_waiting);
}

bool QuicConnection::ShouldLastPacketInstigateAck() const {
  if (!last_stream_frames_.empty() ||
      !last_goaway_frames_.empty() ||
      !last_rst_frames_.empty() ||
      !last_window_update_frames_.empty() ||
      !last_blocked_frames_.empty()) {
    return true;
  }

  if (!last_ack_frames_.empty() &&
      last_ack_frames_.back().received_info.is_truncated) {
    return true;
  }
  return false;
}

void QuicConnection::UpdateStopWaitingCount() {
  if (last_ack_frames_.empty()) {
    return;
  }

  // If the peer is still waiting for a packet that we are no longer planning to
  // send, send an ack to raise the high water mark.
  if (!last_ack_frames_.back().received_info.missing_packets.empty() &&
      GetLeastUnacked() >
          *last_ack_frames_.back().received_info.missing_packets.begin()) {
    ++stop_waiting_count_;
  } else {
    stop_waiting_count_ = 0;
  }
}

QuicPacketSequenceNumber QuicConnection::GetLeastUnacked() const {
  return sent_packet_manager_.HasUnackedPackets() ?
      sent_packet_manager_.GetLeastUnackedSentPacket() :
      packet_generator_.sequence_number() + 1;
}

void QuicConnection::MaybeSendInResponseToPacket() {
  if (!connected_) {
    return;
  }
  ScopedPacketBundler bundler(this, ack_queued_ ? SEND_ACK : NO_ACK);

  // Now that we have received an ack, we might be able to send packets which
  // are queued locally, or drain streams which are blocked.
  if (CanWrite(HAS_RETRANSMITTABLE_DATA)) {
    OnCanWrite();
  }
}

void QuicConnection::SendVersionNegotiationPacket() {
  // TODO(alyssar): implement zero server state negotiation.
  pending_version_negotiation_packet_ = true;
  if (writer_->IsWriteBlocked()) {
    visitor_->OnWriteBlocked();
    return;
  }
  scoped_ptr<QuicEncryptedPacket> version_packet(
      packet_generator_.SerializeVersionNegotiationPacket(
          framer_.supported_versions()));
  WriteResult result = writer_->WritePacket(
      version_packet->data(), version_packet->length(),
      self_address().address(), peer_address());

  if (result.status == WRITE_STATUS_ERROR) {
    // We can't send an error as the socket is presumably borked.
    CloseConnection(QUIC_PACKET_WRITE_ERROR, false);
    return;
  }
  if (result.status == WRITE_STATUS_BLOCKED) {
    visitor_->OnWriteBlocked();
    if (writer_->IsWriteBlockedDataBuffered()) {
      pending_version_negotiation_packet_ = false;
    }
    return;
  }

  pending_version_negotiation_packet_ = false;
}

QuicConsumedData QuicConnection::SendStreamData(
    QuicStreamId id,
    const IOVector& data,
    QuicStreamOffset offset,
    bool fin,
    FecProtection fec_protection,
    QuicAckNotifier::DelegateInterface* delegate) {
  if (!fin && data.Empty()) {
    LOG(DFATAL) << "Attempt to send empty stream frame";
  }

  // This notifier will be owned by the AckNotifierManager (or deleted below if
  // no data or FIN was consumed).
  QuicAckNotifier* notifier = NULL;
  if (delegate) {
    notifier = new QuicAckNotifier(delegate);
  }

  // Opportunistically bundle an ack with every outgoing packet.
  // Particularly, we want to bundle with handshake packets since we don't know
  // which decrypter will be used on an ack packet following a handshake
  // packet (a handshake packet from client to server could result in a REJ or a
  // SHLO from the server, leading to two different decrypters at the server.)
  //
  // TODO(jri): Note that ConsumeData may cause a response packet to be sent.
  // We may end up sending stale ack information if there are undecryptable
  // packets hanging around and/or there are revivable packets which may get
  // handled after this packet is sent. Change ScopedPacketBundler to do the
  // right thing: check ack_queued_, and then check undecryptable packets and
  // also if there is possibility of revival. Only bundle an ack if there's no
  // processing left that may cause received_info_ to change.
  ScopedPacketBundler ack_bundler(this, BUNDLE_PENDING_ACK);
  QuicConsumedData consumed_data =
      packet_generator_.ConsumeData(id, data, offset, fin, fec_protection,
                                    notifier);

  if (notifier &&
      (consumed_data.bytes_consumed == 0 && !consumed_data.fin_consumed)) {
    // No data was consumed, nor was a fin consumed, so delete the notifier.
    delete notifier;
  }

  return consumed_data;
}

void QuicConnection::SendRstStream(QuicStreamId id,
                                   QuicRstStreamErrorCode error,
                                   QuicStreamOffset bytes_written) {
  // Opportunistically bundle an ack with this outgoing packet.
  ScopedPacketBundler ack_bundler(this, BUNDLE_PENDING_ACK);
  packet_generator_.AddControlFrame(QuicFrame(new QuicRstStreamFrame(
      id, AdjustErrorForVersion(error, version()), bytes_written)));
}

void QuicConnection::SendWindowUpdate(QuicStreamId id,
                                      QuicStreamOffset byte_offset) {
  // Opportunistically bundle an ack with this outgoing packet.
  ScopedPacketBundler ack_bundler(this, BUNDLE_PENDING_ACK);
  packet_generator_.AddControlFrame(
      QuicFrame(new QuicWindowUpdateFrame(id, byte_offset)));
}

void QuicConnection::SendBlocked(QuicStreamId id) {
  // Opportunistically bundle an ack with this outgoing packet.
  ScopedPacketBundler ack_bundler(this, BUNDLE_PENDING_ACK);
  packet_generator_.AddControlFrame(QuicFrame(new QuicBlockedFrame(id)));
}

const QuicConnectionStats& QuicConnection::GetStats() {
  // Update rtt and estimated bandwidth.
  stats_.min_rtt_us =
      sent_packet_manager_.GetRttStats()->min_rtt().ToMicroseconds();
  stats_.srtt_us =
      sent_packet_manager_.GetRttStats()->SmoothedRtt().ToMicroseconds();
  stats_.estimated_bandwidth =
      sent_packet_manager_.BandwidthEstimate().ToBytesPerSecond();
  stats_.congestion_window = sent_packet_manager_.GetCongestionWindow();
  stats_.max_packet_size = packet_generator_.max_packet_length();
  return stats_;
}

void QuicConnection::ProcessUdpPacket(const IPEndPoint& self_address,
                                      const IPEndPoint& peer_address,
                                      const QuicEncryptedPacket& packet) {
  if (!connected_) {
    return;
  }
  if (debug_visitor_) {
    debug_visitor_->OnPacketReceived(self_address, peer_address, packet);
  }
  last_packet_revived_ = false;
  last_size_ = packet.length();

  CheckForAddressMigration(self_address, peer_address);

  stats_.bytes_received += packet.length();
  ++stats_.packets_received;

  if (!framer_.ProcessPacket(packet)) {
    // If we are unable to decrypt this packet, it might be
    // because the CHLO or SHLO packet was lost.
    if (encryption_level_ != ENCRYPTION_FORWARD_SECURE &&
        framer_.error() == QUIC_DECRYPTION_FAILURE &&
        undecryptable_packets_.size() < kMaxUndecryptablePackets) {
      QueueUndecryptablePacket(packet);
    }
    DVLOG(1) << ENDPOINT << "Unable to process packet.  Last packet processed: "
             << last_header_.packet_sequence_number;
    return;
  }

  ++stats_.packets_processed;
  MaybeProcessUndecryptablePackets();
  MaybeProcessRevivedPacket();
  MaybeSendInResponseToPacket();
  SetPingAlarm();
}

void QuicConnection::CheckForAddressMigration(
    const IPEndPoint& self_address, const IPEndPoint& peer_address) {
  peer_ip_changed_ = false;
  peer_port_changed_ = false;
  self_ip_changed_ = false;
  self_port_changed_ = false;

  if (peer_address_.address().empty()) {
    peer_address_ = peer_address;
  }
  if (self_address_.address().empty()) {
    self_address_ = self_address;
  }

  if (!peer_address.address().empty() && !peer_address_.address().empty()) {
    peer_ip_changed_ = (peer_address.address() != peer_address_.address());
    peer_port_changed_ = (peer_address.port() != peer_address_.port());

    // Store in case we want to migrate connection in ProcessValidatedPacket.
    migrating_peer_port_ = peer_address.port();
  }

  if (!self_address.address().empty() && !self_address_.address().empty()) {
    self_ip_changed_ = (self_address.address() != self_address_.address());
    self_port_changed_ = (self_address.port() != self_address_.port());
  }
}

void QuicConnection::OnCanWrite() {
  DCHECK(!writer_->IsWriteBlocked());

  WriteQueuedPackets();
  WritePendingRetransmissions();

  // Sending queued packets may have caused the socket to become write blocked,
  // or the congestion manager to prohibit sending.  If we've sent everything
  // we had queued and we're still not blocked, let the visitor know it can
  // write more.
  if (!CanWrite(HAS_RETRANSMITTABLE_DATA)) {
    return;
  }

  {  // Limit the scope of the bundler.
    // Set |include_ack| to false in bundler; ack inclusion happens elsewhere.
    ScopedPacketBundler bundler(this, NO_ACK);
    visitor_->OnCanWrite();
  }

  // After the visitor writes, it may have caused the socket to become write
  // blocked or the congestion manager to prohibit sending, so check again.
  if (visitor_->WillingAndAbleToWrite() &&
      !resume_writes_alarm_->IsSet() &&
      CanWrite(HAS_RETRANSMITTABLE_DATA)) {
    // We're not write blocked, but some stream didn't write out all of its
    // bytes. Register for 'immediate' resumption so we'll keep writing after
    // other connections and events have had a chance to use the thread.
    resume_writes_alarm_->Set(clock_->ApproximateNow());
  }
}

void QuicConnection::WriteIfNotBlocked() {
  if (!writer_->IsWriteBlocked()) {
    OnCanWrite();
  }
}

bool QuicConnection::ProcessValidatedPacket() {
  if ((!FLAGS_quic_allow_port_migration && peer_port_changed_) ||
      peer_ip_changed_ || self_ip_changed_ || self_port_changed_) {
    SendConnectionCloseWithDetails(
        QUIC_ERROR_MIGRATING_ADDRESS,
        "Neither IP address migration, nor self port migration are supported.");
    return false;
  }

  // Port migration is supported, do it now if port has changed.
  if (FLAGS_quic_allow_port_migration &&
      peer_port_changed_) {
    DVLOG(1) << ENDPOINT << "Peer's port changed from "
             << peer_address_.port() << " to " << migrating_peer_port_
             << ", migrating connection.";
    peer_address_ = IPEndPoint(peer_address_.address(), migrating_peer_port_);
  }

  time_of_last_received_packet_ = clock_->Now();
  DVLOG(1) << ENDPOINT << "time of last received packet: "
           << time_of_last_received_packet_.ToDebuggingValue();

  if (is_server_ && encryption_level_ == ENCRYPTION_NONE &&
      last_size_ > packet_generator_.max_packet_length()) {
    packet_generator_.set_max_packet_length(last_size_);
  }
  return true;
}

void QuicConnection::WriteQueuedPackets() {
  DCHECK(!writer_->IsWriteBlocked());

  if (pending_version_negotiation_packet_) {
    SendVersionNegotiationPacket();
  }

  QueuedPacketList::iterator packet_iterator = queued_packets_.begin();
  while (!writer_->IsWriteBlocked() &&
         packet_iterator != queued_packets_.end()) {
    if (WritePacket(*packet_iterator)) {
      delete packet_iterator->packet;
      packet_iterator = queued_packets_.erase(packet_iterator);
    } else {
      // Continue, because some queued packets may still be writable.
      // This can happen if a retransmit send fails.
      ++packet_iterator;
    }
  }
}

void QuicConnection::WritePendingRetransmissions() {
  // Keep writing as long as there's a pending retransmission which can be
  // written.
  while (sent_packet_manager_.HasPendingRetransmissions()) {
    const QuicSentPacketManager::PendingRetransmission pending =
        sent_packet_manager_.NextPendingRetransmission();
    if (GetPacketType(&pending.retransmittable_frames) == NORMAL &&
        !CanWrite(HAS_RETRANSMITTABLE_DATA)) {
      break;
    }

    // Re-packetize the frames with a new sequence number for retransmission.
    // Retransmitted data packets do not use FEC, even when it's enabled.
    // Retransmitted packets use the same sequence number length as the
    // original.
    // Flush the packet generator before making a new packet.
    // TODO(ianswett): Implement ReserializeAllFrames as a separate path that
    // does not require the creator to be flushed.
    packet_generator_.FlushAllQueuedFrames();
    SerializedPacket serialized_packet = packet_generator_.ReserializeAllFrames(
        pending.retransmittable_frames.frames(),
        pending.sequence_number_length);

    DVLOG(1) << ENDPOINT << "Retransmitting " << pending.sequence_number
             << " as " << serialized_packet.sequence_number;
    if (debug_visitor_) {
      debug_visitor_->OnPacketRetransmitted(
          pending.sequence_number, serialized_packet.sequence_number);
    }
    sent_packet_manager_.OnRetransmittedPacket(
        pending.sequence_number,
        serialized_packet.sequence_number);

    SendOrQueuePacket(pending.retransmittable_frames.encryption_level(),
                      serialized_packet,
                      pending.transmission_type);
  }
}

void QuicConnection::RetransmitUnackedPackets(
    RetransmissionType retransmission_type) {
  sent_packet_manager_.RetransmitUnackedPackets(retransmission_type);

  WriteIfNotBlocked();
}

void QuicConnection::NeuterUnencryptedPackets() {
  sent_packet_manager_.NeuterUnencryptedPackets();
  // This may have changed the retransmission timer, so re-arm it.
  retransmission_alarm_->Cancel();
  QuicTime retransmission_time = sent_packet_manager_.GetRetransmissionTime();
  if (retransmission_time != QuicTime::Zero()) {
    retransmission_alarm_->Set(retransmission_time);
  }
}

bool QuicConnection::ShouldGeneratePacket(
    TransmissionType transmission_type,
    HasRetransmittableData retransmittable,
    IsHandshake handshake) {
  // We should serialize handshake packets immediately to ensure that they
  // end up sent at the right encryption level.
  if (handshake == IS_HANDSHAKE) {
    return true;
  }

  return CanWrite(retransmittable);
}

bool QuicConnection::CanWrite(HasRetransmittableData retransmittable) {
  if (writer_->IsWriteBlocked()) {
    visitor_->OnWriteBlocked();
    return false;
  }

  send_alarm_->Cancel();
  QuicTime now = clock_->Now();
  QuicTime::Delta delay = sent_packet_manager_.TimeUntilSend(
      now, retransmittable);
  if (delay.IsInfinite()) {
    return false;
  }

  // If the scheduler requires a delay, then we can not send this packet now.
  if (!delay.IsZero()) {
    send_alarm_->Set(now.Add(delay));
    DVLOG(1) << "Delaying sending.";
    return false;
  }
  return true;
}

bool QuicConnection::WritePacket(QueuedPacket packet) {
  QuicPacketSequenceNumber sequence_number = packet.sequence_number;
  if (ShouldDiscardPacket(packet.encryption_level,
                          sequence_number,
                          packet.retransmittable)) {
    ++stats_.packets_discarded;
    return true;
  }

  // If the packet is CONNECTION_CLOSE, we need to try to send it immediately
  // and encrypt it to hand it off to TimeWaitListManager.
  // If the packet is QUEUED, we don't re-consult the congestion control.
  // This ensures packets are sent in sequence number order.
  // TODO(ianswett): The congestion control should have been consulted before
  // serializing the packet, so this could be turned into a LOG_IF(DFATAL).
  if (packet.type == NORMAL && !CanWrite(packet.retransmittable)) {
    return false;
  }

  // Some encryption algorithms require the packet sequence numbers not be
  // repeated.
  DCHECK_LE(sequence_number_of_last_sent_packet_, sequence_number);
  sequence_number_of_last_sent_packet_ = sequence_number;

  QuicEncryptedPacket* encrypted = framer_.EncryptPacket(
      packet.encryption_level, sequence_number, *packet.packet);
  if (encrypted == NULL) {
    LOG(DFATAL) << ENDPOINT << "Failed to encrypt packet number "
                << sequence_number;
    // CloseConnection does not send close packet, so no infinite loop here.
    CloseConnection(QUIC_ENCRYPTION_FAILURE, false);
    return false;
  }

  // Connection close packets are eventually owned by TimeWaitListManager.
  // Others are deleted at the end of this call.
  scoped_ptr<QuicEncryptedPacket> encrypted_deleter;
  if (packet.type == CONNECTION_CLOSE) {
    DCHECK(connection_close_packet_.get() == NULL);
    connection_close_packet_.reset(encrypted);
    // This assures we won't try to write *forced* packets when blocked.
    // Return true to stop processing.
    if (writer_->IsWriteBlocked()) {
      visitor_->OnWriteBlocked();
      return true;
    }
  } else {
    encrypted_deleter.reset(encrypted);
  }

  LOG_IF(DFATAL, encrypted->length() >
         packet_generator_.max_packet_length())
      << "Writing an encrypted packet larger than max_packet_length:"
      << packet_generator_.max_packet_length() << " encrypted length: "
      << encrypted->length();
  DVLOG(1) << ENDPOINT << "Sending packet " << sequence_number
           << " : " << (packet.packet->is_fec_packet() ? "FEC " :
               (packet.retransmittable == HAS_RETRANSMITTABLE_DATA
                    ? "data bearing " : " ack only "))
           << ", encryption level: "
           << QuicUtils::EncryptionLevelToString(packet.encryption_level)
           << ", length:" << packet.packet->length() << ", encrypted length:"
           << encrypted->length();
  DVLOG(2) << ENDPOINT << "packet(" << sequence_number << "): " << std::endl
           << QuicUtils::StringToHexASCIIDump(packet.packet->AsStringPiece());

  DCHECK(encrypted->length() <= kMaxPacketSize ||
         FLAGS_quic_allow_oversized_packets_for_test)
      << "Packet " << sequence_number << " will not be read; too large: "
      << packet.packet->length() << " " << encrypted->length() << " "
      << " close: " << (packet.type == CONNECTION_CLOSE ? "yes" : "no");

  DCHECK(pending_write_.get() == NULL);
  pending_write_.reset(new QueuedPacket(packet));

  WriteResult result = writer_->WritePacket(encrypted->data(),
                                            encrypted->length(),
                                            self_address().address(),
                                            peer_address());
  if (result.error_code == ERR_IO_PENDING) {
    DCHECK_EQ(WRITE_STATUS_BLOCKED, result.status);
  }
  if (debug_visitor_) {
    // Pass the write result to the visitor.
    debug_visitor_->OnPacketSent(sequence_number,
                                 packet.encryption_level,
                                 packet.transmission_type,
                                 *encrypted,
                                 result);
  }
  if (result.status == WRITE_STATUS_BLOCKED) {
    visitor_->OnWriteBlocked();
    // If the socket buffers the the data, then the packet should not
    // be queued and sent again, which would result in an unnecessary
    // duplicate packet being sent.  The helper must call OnPacketSent
    // when the packet is actually sent.
    if (writer_->IsWriteBlockedDataBuffered()) {
      return true;
    }
    pending_write_.reset();
    return false;
  }

  if (OnPacketSent(result)) {
    return true;
  }
  return false;
}

bool QuicConnection::ShouldDiscardPacket(
    EncryptionLevel level,
    QuicPacketSequenceNumber sequence_number,
    HasRetransmittableData retransmittable) {
  if (!connected_) {
    DVLOG(1) << ENDPOINT
             << "Not sending packet as connection is disconnected.";
    return true;
  }

  // If the packet has been discarded before sending, don't send it.
  // This occurs if a packet gets serialized, queued, then discarded.
  if (!sent_packet_manager_.IsUnacked(sequence_number)) {
    DVLOG(1) << ENDPOINT << "Dropping packet before sending: "
             << sequence_number << " since it has already been discarded.";
    return true;
  }

  if (encryption_level_ == ENCRYPTION_FORWARD_SECURE &&
      level == ENCRYPTION_NONE) {
    // Drop packets that are NULL encrypted since the peer won't accept them
    // anymore.
    DVLOG(1) << ENDPOINT << "Dropping NULL encrypted packet: "
             << sequence_number << " since the connection is forward secure.";
    LOG_IF(DFATAL,
           sent_packet_manager_.HasRetransmittableFrames(sequence_number))
        << "Once forward secure, all NULL encrypted packets should be "
        << "neutered.";
    return true;
  }

  if (retransmittable == HAS_RETRANSMITTABLE_DATA &&
      !sent_packet_manager_.HasRetransmittableFrames(sequence_number)) {
    DVLOG(1) << ENDPOINT << "Dropping unacked packet: " << sequence_number
             << " A previous transmission was acked while write blocked.";
    return true;
  }

  return false;
}

bool QuicConnection::OnPacketSent(WriteResult result) {
  DCHECK_NE(WRITE_STATUS_BLOCKED, result.status);
  if (pending_write_.get() == NULL) {
    LOG(DFATAL) << "OnPacketSent called without a pending write.";
    return false;
  }

  QuicPacketSequenceNumber sequence_number = pending_write_->sequence_number;
  TransmissionType transmission_type  = pending_write_->transmission_type;
  HasRetransmittableData retransmittable = pending_write_->retransmittable;
  size_t length = pending_write_->length;
  pending_write_.reset();

  if (result.status == WRITE_STATUS_ERROR) {
    DVLOG(1) << "Write failed with error: " << result.error_code << " ("
             << ErrorToString(result.error_code) << ")";
    // We can't send an error as the socket is presumably borked.
    CloseConnection(QUIC_PACKET_WRITE_ERROR, false);
    return false;
  }

  QuicTime now = clock_->Now();
  if (transmission_type == NOT_RETRANSMISSION) {
    time_of_last_sent_new_packet_ = now;
  }
  SetPingAlarm();
  DVLOG(1) << ENDPOINT << "time of last sent packet: "
           << now.ToDebuggingValue();

  // TODO(ianswett): Change the sequence number length and other packet creator
  // options by a more explicit API than setting a struct value directly.
  packet_generator_.UpdateSequenceNumberLength(
      received_packet_manager_.least_packet_awaited_by_peer(),
      sent_packet_manager_.GetCongestionWindow());

  bool reset_retransmission_alarm =
      sent_packet_manager_.OnPacketSent(sequence_number, now, length,
                                        transmission_type, retransmittable);

  if (reset_retransmission_alarm || !retransmission_alarm_->IsSet()) {
    retransmission_alarm_->Cancel();
    QuicTime retransmission_time = sent_packet_manager_.GetRetransmissionTime();
    if (retransmission_time != QuicTime::Zero()) {
      retransmission_alarm_->Set(retransmission_time);
    }
  }

  stats_.bytes_sent += result.bytes_written;
  ++stats_.packets_sent;

  if (transmission_type != NOT_RETRANSMISSION) {
    stats_.bytes_retransmitted += result.bytes_written;
    ++stats_.packets_retransmitted;
  }

  return true;
}

bool QuicConnection::OnSerializedPacket(
    const SerializedPacket& serialized_packet) {
  if (serialized_packet.retransmittable_frames) {
    serialized_packet.retransmittable_frames->
        set_encryption_level(encryption_level_);
  }
  sent_packet_manager_.OnSerializedPacket(serialized_packet);
  // The TransmissionType is NOT_RETRANSMISSION because all retransmissions
  // serialize packets and invoke SendOrQueuePacket directly.
  return SendOrQueuePacket(encryption_level_,
                           serialized_packet,
                           NOT_RETRANSMISSION);
}

bool QuicConnection::SendOrQueuePacket(EncryptionLevel level,
                                       const SerializedPacket& packet,
                                       TransmissionType transmission_type) {
  if (packet.packet == NULL) {
    LOG(DFATAL) << "NULL packet passed in to SendOrQueuePacket";
    return true;
  }

  sent_entropy_manager_.RecordPacketEntropyHash(packet.sequence_number,
                                                packet.entropy_hash);
  QueuedPacket queued_packet(packet, level, transmission_type);
  // If there are already queued packets, put this at the end,
  // unless it's ConnectionClose, in which case it is written immediately.
  if ((queued_packet.type == CONNECTION_CLOSE || queued_packets_.empty()) &&
      WritePacket(queued_packet)) {
    delete packet.packet;
    return true;
  }
  queued_packet.type = QUEUED;
  queued_packets_.push_back(queued_packet);
  return false;
}

void QuicConnection::UpdateStopWaiting(QuicStopWaitingFrame* stop_waiting) {
  stop_waiting->least_unacked = GetLeastUnacked();
  stop_waiting->entropy_hash = sent_entropy_manager_.EntropyHash(
      stop_waiting->least_unacked - 1);
}

void QuicConnection::SendPing() {
  if (retransmission_alarm_->IsSet()) {
    return;
  }
  if (version() <= QUIC_VERSION_17) {
    // TODO(rch): remove this when we remove version 17.
    // This is a horrible hideous hack which we should not support.
    IOVector data;
    char c_data[] = "C";
    data.Append(c_data, 1);
    QuicConsumedData consumed_data =
        packet_generator_.ConsumeData(kCryptoStreamId, data, 0, false,
                                      MAY_FEC_PROTECT, NULL);
    if (consumed_data.bytes_consumed == 0) {
      DLOG(ERROR) << "Unable to send ping!?";
    }
  } else {
    packet_generator_.AddControlFrame(QuicFrame(new QuicPingFrame));
  }
}

void QuicConnection::SendAck() {
  ack_alarm_->Cancel();
  stop_waiting_count_ = 0;
  // TODO(rch): delay this until the CreateFeedbackFrame
  // method is invoked.  This requires changes SetShouldSendAck
  // to be a no-arg method, and re-jiggering its implementation.
  bool send_feedback = false;
  if (received_packet_manager_.GenerateCongestionFeedback(
          &outgoing_congestion_feedback_)) {
    DVLOG(1) << ENDPOINT << "Sending feedback: "
             << outgoing_congestion_feedback_;
    send_feedback = true;
  }

  packet_generator_.SetShouldSendAck(send_feedback,
                                     version() > QUIC_VERSION_15);
}

void QuicConnection::OnRetransmissionTimeout() {
  if (!sent_packet_manager_.HasUnackedPackets()) {
    return;
  }

  sent_packet_manager_.OnRetransmissionTimeout();
  WriteIfNotBlocked();
  // In the TLP case, the SentPacketManager gives the connection the opportunity
  // to send new data before retransmitting.
  if (sent_packet_manager_.MaybeRetransmitTailLossProbe()) {
    // Send the pending retransmission now that it's been queued.
    WriteIfNotBlocked();
  }

  // Ensure the retransmission alarm is always set if there are unacked packets
  // and nothing waiting to be sent.
  if (!HasQueuedData() && !retransmission_alarm_->IsSet()) {
    QuicTime rto_timeout = sent_packet_manager_.GetRetransmissionTime();
    if (rto_timeout != QuicTime::Zero()) {
      retransmission_alarm_->Set(rto_timeout);
    }
  }
}

void QuicConnection::SetEncrypter(EncryptionLevel level,
                                  QuicEncrypter* encrypter) {
  framer_.SetEncrypter(level, encrypter);
}

const QuicEncrypter* QuicConnection::encrypter(EncryptionLevel level) const {
  return framer_.encrypter(level);
}

void QuicConnection::SetDefaultEncryptionLevel(EncryptionLevel level) {
  encryption_level_ = level;
  packet_generator_.set_encryption_level(level);
}

void QuicConnection::SetDecrypter(QuicDecrypter* decrypter,
                                  EncryptionLevel level) {
  framer_.SetDecrypter(decrypter, level);
}

void QuicConnection::SetAlternativeDecrypter(QuicDecrypter* decrypter,
                                             EncryptionLevel level,
                                             bool latch_once_used) {
  framer_.SetAlternativeDecrypter(decrypter, level, latch_once_used);
}

const QuicDecrypter* QuicConnection::decrypter() const {
  return framer_.decrypter();
}

const QuicDecrypter* QuicConnection::alternative_decrypter() const {
  return framer_.alternative_decrypter();
}

void QuicConnection::QueueUndecryptablePacket(
    const QuicEncryptedPacket& packet) {
  DVLOG(1) << ENDPOINT << "Queueing undecryptable packet.";
  undecryptable_packets_.push_back(packet.Clone());
}

void QuicConnection::MaybeProcessUndecryptablePackets() {
  if (undecryptable_packets_.empty() || encryption_level_ == ENCRYPTION_NONE) {
    return;
  }

  while (connected_ && !undecryptable_packets_.empty()) {
    DVLOG(1) << ENDPOINT << "Attempting to process undecryptable packet";
    QuicEncryptedPacket* packet = undecryptable_packets_.front();
    if (!framer_.ProcessPacket(*packet) &&
        framer_.error() == QUIC_DECRYPTION_FAILURE) {
      DVLOG(1) << ENDPOINT << "Unable to process undecryptable packet...";
      break;
    }
    DVLOG(1) << ENDPOINT << "Processed undecryptable packet!";
    ++stats_.packets_processed;
    delete packet;
    undecryptable_packets_.pop_front();
  }

  // Once forward secure encryption is in use, there will be no
  // new keys installed and hence any undecryptable packets will
  // never be able to be decrypted.
  if (encryption_level_ == ENCRYPTION_FORWARD_SECURE) {
    STLDeleteElements(&undecryptable_packets_);
  }
}

void QuicConnection::MaybeProcessRevivedPacket() {
  QuicFecGroup* group = GetFecGroup();
  if (!connected_ || group == NULL || !group->CanRevive()) {
    return;
  }
  QuicPacketHeader revived_header;
  char revived_payload[kMaxPacketSize];
  size_t len = group->Revive(&revived_header, revived_payload, kMaxPacketSize);
  revived_header.public_header.connection_id = connection_id_;
  revived_header.public_header.connection_id_length =
      last_header_.public_header.connection_id_length;
  revived_header.public_header.version_flag = false;
  revived_header.public_header.reset_flag = false;
  revived_header.public_header.sequence_number_length =
      last_header_.public_header.sequence_number_length;
  revived_header.fec_flag = false;
  revived_header.is_in_fec_group = NOT_IN_FEC_GROUP;
  revived_header.fec_group = 0;
  group_map_.erase(last_header_.fec_group);
  last_decrypted_packet_level_ = group->effective_encryption_level();
  DCHECK_LT(last_decrypted_packet_level_, NUM_ENCRYPTION_LEVELS);
  delete group;

  last_packet_revived_ = true;
  if (debug_visitor_) {
    debug_visitor_->OnRevivedPacket(revived_header,
                                    StringPiece(revived_payload, len));
  }

  ++stats_.packets_revived;
  framer_.ProcessRevivedPacket(&revived_header,
                               StringPiece(revived_payload, len));
}

QuicFecGroup* QuicConnection::GetFecGroup() {
  QuicFecGroupNumber fec_group_num = last_header_.fec_group;
  if (fec_group_num == 0) {
    return NULL;
  }
  if (group_map_.count(fec_group_num) == 0) {
    if (group_map_.size() >= kMaxFecGroups) {  // Too many groups
      if (fec_group_num < group_map_.begin()->first) {
        // The group being requested is a group we've seen before and deleted.
        // Don't recreate it.
        return NULL;
      }
      // Clear the lowest group number.
      delete group_map_.begin()->second;
      group_map_.erase(group_map_.begin());
    }
    group_map_[fec_group_num] = new QuicFecGroup();
  }
  return group_map_[fec_group_num];
}

void QuicConnection::SendConnectionClose(QuicErrorCode error) {
  SendConnectionCloseWithDetails(error, string());
}

void QuicConnection::SendConnectionCloseWithDetails(QuicErrorCode error,
                                                    const string& details) {
  // If we're write blocked, WritePacket() will not send, but will capture the
  // serialized packet.
  SendConnectionClosePacket(error, details);
  if (connected_) {
    // It's possible that while sending the connection close packet, we get a
    // socket error and disconnect right then and there.  Avoid a double
    // disconnect in that case.
    CloseConnection(error, false);
  }
}

void QuicConnection::SendConnectionClosePacket(QuicErrorCode error,
                                               const string& details) {
  DVLOG(1) << ENDPOINT << "Force closing " << connection_id()
           << " with error " << QuicUtils::ErrorToString(error)
           << " (" << error << ") " << details;
  ScopedPacketBundler ack_bundler(this, SEND_ACK);
  QuicConnectionCloseFrame* frame = new QuicConnectionCloseFrame();
  frame->error_code = error;
  frame->error_details = details;
  packet_generator_.AddControlFrame(QuicFrame(frame));
  packet_generator_.FlushAllQueuedFrames();
}

void QuicConnection::CloseConnection(QuicErrorCode error, bool from_peer) {
  if (!connected_) {
    DLOG(DFATAL) << "Error: attempt to close an already closed connection"
                 << base::debug::StackTrace().ToString();
    return;
  }
  connected_ = false;
  visitor_->OnConnectionClosed(error, from_peer);
  // Cancel the alarms so they don't trigger any action now that the
  // connection is closed.
  ack_alarm_->Cancel();
  resume_writes_alarm_->Cancel();
  retransmission_alarm_->Cancel();
  send_alarm_->Cancel();
  timeout_alarm_->Cancel();
}

void QuicConnection::SendGoAway(QuicErrorCode error,
                                QuicStreamId last_good_stream_id,
                                const string& reason) {
  DVLOG(1) << ENDPOINT << "Going away with error "
           << QuicUtils::ErrorToString(error)
           << " (" << error << ")";

  // Opportunistically bundle an ack with this outgoing packet.
  ScopedPacketBundler ack_bundler(this, BUNDLE_PENDING_ACK);
  packet_generator_.AddControlFrame(
      QuicFrame(new QuicGoAwayFrame(error, last_good_stream_id, reason)));
}

void QuicConnection::CloseFecGroupsBefore(
    QuicPacketSequenceNumber sequence_number) {
  FecGroupMap::iterator it = group_map_.begin();
  while (it != group_map_.end()) {
    // If this is the current group or the group doesn't protect this packet
    // we can ignore it.
    if (last_header_.fec_group == it->first ||
        !it->second->ProtectsPacketsBefore(sequence_number)) {
      ++it;
      continue;
    }
    QuicFecGroup* fec_group = it->second;
    DCHECK(!fec_group->CanRevive());
    FecGroupMap::iterator next = it;
    ++next;
    group_map_.erase(it);
    delete fec_group;
    it = next;
  }
}

size_t QuicConnection::max_packet_length() const {
  return packet_generator_.max_packet_length();
}

void QuicConnection::set_max_packet_length(size_t length) {
  return packet_generator_.set_max_packet_length(length);
}

bool QuicConnection::HasQueuedData() const {
  return pending_version_negotiation_packet_ ||
      !queued_packets_.empty() || packet_generator_.HasQueuedFrames();
}

bool QuicConnection::CanWriteStreamData() {
  // Don't write stream data if there are negotiation or queued data packets
  // to send. Otherwise, continue and bundle as many frames as possible.
  if (pending_version_negotiation_packet_ || !queued_packets_.empty()) {
    return false;
  }

  IsHandshake pending_handshake = visitor_->HasPendingHandshake() ?
      IS_HANDSHAKE : NOT_HANDSHAKE;
  // Sending queued packets may have caused the socket to become write blocked,
  // or the congestion manager to prohibit sending.  If we've sent everything
  // we had queued and we're still not blocked, let the visitor know it can
  // write more.
  return ShouldGeneratePacket(NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA,
                              pending_handshake);
}

void QuicConnection::SetIdleNetworkTimeout(QuicTime::Delta timeout) {
  if (timeout < idle_network_timeout_) {
    idle_network_timeout_ = timeout;
    CheckForTimeout();
  } else {
    idle_network_timeout_ = timeout;
  }
}

void QuicConnection::SetOverallConnectionTimeout(QuicTime::Delta timeout) {
  if (timeout < overall_connection_timeout_) {
    overall_connection_timeout_ = timeout;
    CheckForTimeout();
  } else {
    overall_connection_timeout_ = timeout;
  }
}

bool QuicConnection::CheckForTimeout() {
  QuicTime now = clock_->ApproximateNow();
  QuicTime time_of_last_packet = max(time_of_last_received_packet_,
                                     time_of_last_sent_new_packet_);

  // |delta| can be < 0 as |now| is approximate time but |time_of_last_packet|
  // is accurate time. However, this should not change the behavior of
  // timeout handling.
  QuicTime::Delta delta = now.Subtract(time_of_last_packet);
  DVLOG(1) << ENDPOINT << "last packet "
           << time_of_last_packet.ToDebuggingValue()
           << " now:" << now.ToDebuggingValue()
           << " delta:" << delta.ToMicroseconds()
           << " network_timeout: " << idle_network_timeout_.ToMicroseconds();
  if (delta >= idle_network_timeout_) {
    DVLOG(1) << ENDPOINT << "Connection timedout due to no network activity.";
    SendConnectionClose(QUIC_CONNECTION_TIMED_OUT);
    return true;
  }

  // Next timeout delta.
  QuicTime::Delta timeout = idle_network_timeout_.Subtract(delta);

  if (!overall_connection_timeout_.IsInfinite()) {
    QuicTime::Delta connected_time =
        now.Subtract(stats_.connection_creation_time);
    DVLOG(1) << ENDPOINT << "connection time: "
             << connected_time.ToMilliseconds() << " overall timeout: "
             << overall_connection_timeout_.ToMilliseconds();
    if (connected_time >= overall_connection_timeout_) {
      DVLOG(1) << ENDPOINT <<
          "Connection timedout due to overall connection timeout.";
      SendConnectionClose(QUIC_CONNECTION_TIMED_OUT);
      return true;
    }

    // Take the min timeout.
    QuicTime::Delta connection_timeout =
        overall_connection_timeout_.Subtract(connected_time);
    if (connection_timeout < timeout) {
      timeout = connection_timeout;
    }
  }

  timeout_alarm_->Cancel();
  timeout_alarm_->Set(clock_->ApproximateNow().Add(timeout));
  return false;
}

void QuicConnection::SetPingAlarm() {
  if (is_server_) {
    // Only clients send pings.
    return;
  }
  ping_alarm_->Cancel();
  if (!visitor_->HasOpenDataStreams()) {
    // Don't send a ping unless there are open streams.
    return;
  }
  QuicTime::Delta ping_timeout = QuicTime::Delta::FromSeconds(kPingTimeoutSecs);
  ping_alarm_->Set(clock_->ApproximateNow().Add(ping_timeout));
}

QuicConnection::ScopedPacketBundler::ScopedPacketBundler(
    QuicConnection* connection,
    AckBundling send_ack)
    : connection_(connection),
      already_in_batch_mode_(connection != NULL &&
                             connection->packet_generator_.InBatchMode()) {
  if (connection_  == NULL) {
    return;
  }
  // Move generator into batch mode. If caller wants us to include an ack,
  // check the delayed-ack timer to see if there's ack info to be sent.
  if (!already_in_batch_mode_) {
    DVLOG(1) << "Entering Batch Mode.";
    connection_->packet_generator_.StartBatchOperations();
  }
  // Bundle an ack if the alarm is set or with every second packet if we need to
  // raise the peer's least unacked.
  bool ack_pending =
      connection_->ack_alarm_->IsSet() || connection_->stop_waiting_count_ > 1;
  if (send_ack == SEND_ACK || (send_ack == BUNDLE_PENDING_ACK && ack_pending)) {
    DVLOG(1) << "Bundling ack with outgoing packet.";
    connection_->SendAck();
  }
}

QuicConnection::ScopedPacketBundler::~ScopedPacketBundler() {
  if (connection_  == NULL) {
    return;
  }
  // If we changed the generator's batch state, restore original batch state.
  if (!already_in_batch_mode_) {
    DVLOG(1) << "Leaving Batch Mode.";
    connection_->packet_generator_.FinishBatchOperations();
  }
  DCHECK_EQ(already_in_batch_mode_,
            connection_->packet_generator_.InBatchMode());
}

}  // namespace net
