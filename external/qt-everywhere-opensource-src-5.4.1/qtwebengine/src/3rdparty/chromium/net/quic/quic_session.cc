// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session.h"

#include "base/stl_util.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_flow_controller.h"
#include "net/quic/quic_headers_stream.h"
#include "net/ssl/ssl_info.h"

using base::StringPiece;
using base::hash_map;
using base::hash_set;
using std::make_pair;
using std::vector;

namespace net {

#define ENDPOINT (is_server() ? "Server: " : " Client: ")

// We want to make sure we delete any closed streams in a safe manner.
// To avoid deleting a stream in mid-operation, we have a simple shim between
// us and the stream, so we can delete any streams when we return from
// processing.
//
// We could just override the base methods, but this makes it easier to make
// sure we don't miss any.
class VisitorShim : public QuicConnectionVisitorInterface {
 public:
  explicit VisitorShim(QuicSession* session) : session_(session) {}

  virtual void OnStreamFrames(const vector<QuicStreamFrame>& frames) OVERRIDE {
    session_->OnStreamFrames(frames);
    session_->PostProcessAfterData();
  }
  virtual void OnRstStream(const QuicRstStreamFrame& frame) OVERRIDE {
    session_->OnRstStream(frame);
    session_->PostProcessAfterData();
  }

  virtual void OnGoAway(const QuicGoAwayFrame& frame) OVERRIDE {
    session_->OnGoAway(frame);
    session_->PostProcessAfterData();
  }

  virtual void OnWindowUpdateFrames(const vector<QuicWindowUpdateFrame>& frames)
      OVERRIDE {
    session_->OnWindowUpdateFrames(frames);
    session_->PostProcessAfterData();
  }

  virtual void OnBlockedFrames(const vector<QuicBlockedFrame>& frames)
      OVERRIDE {
    session_->OnBlockedFrames(frames);
    session_->PostProcessAfterData();
  }

  virtual void OnCanWrite() OVERRIDE {
    session_->OnCanWrite();
    session_->PostProcessAfterData();
  }

  virtual void OnSuccessfulVersionNegotiation(
      const QuicVersion& version) OVERRIDE {
    session_->OnSuccessfulVersionNegotiation(version);
  }

  virtual void OnConnectionClosed(
      QuicErrorCode error, bool from_peer) OVERRIDE {
    session_->OnConnectionClosed(error, from_peer);
    // The session will go away, so don't bother with cleanup.
  }

  virtual void OnWriteBlocked() OVERRIDE {
    session_->OnWriteBlocked();
  }

  virtual bool WillingAndAbleToWrite() const OVERRIDE {
    return session_->WillingAndAbleToWrite();
  }

  virtual bool HasPendingHandshake() const OVERRIDE {
    return session_->HasPendingHandshake();
  }

  virtual bool HasOpenDataStreams() const OVERRIDE {
    return session_->HasOpenDataStreams();
  }

 private:
  QuicSession* session_;
};

QuicSession::QuicSession(QuicConnection* connection, const QuicConfig& config)
    : connection_(connection),
      visitor_shim_(new VisitorShim(this)),
      config_(config),
      max_open_streams_(config_.max_streams_per_connection()),
      next_stream_id_(is_server() ? 2 : 3),
      largest_peer_created_stream_id_(0),
      error_(QUIC_NO_ERROR),
      goaway_received_(false),
      goaway_sent_(false),
      has_pending_handshake_(false) {
  if (connection_->version() <= QUIC_VERSION_19) {
    flow_controller_.reset(new QuicFlowController(
        connection_.get(), 0, is_server(), kDefaultFlowControlSendWindow,
        config_.GetInitialFlowControlWindowToSend(),
        config_.GetInitialFlowControlWindowToSend()));
  } else {
    flow_controller_.reset(new QuicFlowController(
        connection_.get(), 0, is_server(), kDefaultFlowControlSendWindow,
        config_.GetInitialSessionFlowControlWindowToSend(),
        config_.GetInitialSessionFlowControlWindowToSend()));
  }

  connection_->set_visitor(visitor_shim_.get());
  connection_->SetFromConfig(config_);
  if (connection_->connected()) {
    connection_->SetOverallConnectionTimeout(
        config_.max_time_before_crypto_handshake());
  }
  headers_stream_.reset(new QuicHeadersStream(this));
  if (!is_server()) {
    // For version above QUIC v12, the headers stream is stream 3, so the
    // next available local stream ID should be 5.
    DCHECK_EQ(kHeadersStreamId, next_stream_id_);
    next_stream_id_ += 2;
  }
}

QuicSession::~QuicSession() {
  STLDeleteElements(&closed_streams_);
  STLDeleteValues(&stream_map_);

  DLOG_IF(WARNING,
          locally_closed_streams_highest_offset_.size() > max_open_streams_)
      << "Surprisingly high number of locally closed streams still waiting for "
         "final byte offset: " << locally_closed_streams_highest_offset_.size();
}

void QuicSession::OnStreamFrames(const vector<QuicStreamFrame>& frames) {
  for (size_t i = 0; i < frames.size(); ++i) {
    // TODO(rch) deal with the error case of stream id 0.
    const QuicStreamFrame& frame = frames[i];
    QuicStreamId stream_id = frame.stream_id;
    ReliableQuicStream* stream = GetStream(stream_id);
    if (!stream) {
      // The stream no longer exists, but we may still be interested in the
      // final stream byte offset sent by the peer. A frame with a FIN can give
      // us this offset.
      if (frame.fin) {
        QuicStreamOffset final_byte_offset =
            frame.offset + frame.data.TotalBufferSize();
        UpdateFlowControlOnFinalReceivedByteOffset(stream_id,
                                                   final_byte_offset);
      }

      continue;
    }
    stream->OnStreamFrame(frames[i]);
  }
}

void QuicSession::OnStreamHeaders(QuicStreamId stream_id,
                                  StringPiece headers_data) {
  QuicDataStream* stream = GetDataStream(stream_id);
  if (!stream) {
    // It's quite possible to receive headers after a stream has been reset.
    return;
  }
  stream->OnStreamHeaders(headers_data);
}

void QuicSession::OnStreamHeadersPriority(QuicStreamId stream_id,
                                          QuicPriority priority) {
  QuicDataStream* stream = GetDataStream(stream_id);
  if (!stream) {
    // It's quite possible to receive headers after a stream has been reset.
    return;
  }
  stream->OnStreamHeadersPriority(priority);
}

void QuicSession::OnStreamHeadersComplete(QuicStreamId stream_id,
                                          bool fin,
                                          size_t frame_len) {
  QuicDataStream* stream = GetDataStream(stream_id);
  if (!stream) {
    // It's quite possible to receive headers after a stream has been reset.
    return;
  }
  stream->OnStreamHeadersComplete(fin, frame_len);
}

void QuicSession::OnRstStream(const QuicRstStreamFrame& frame) {
  if (frame.stream_id == kCryptoStreamId) {
    connection()->SendConnectionCloseWithDetails(
        QUIC_INVALID_STREAM_ID,
        "Attempt to reset the crypto stream");
    return;
  }
  if (frame.stream_id == kHeadersStreamId) {
    connection()->SendConnectionCloseWithDetails(
        QUIC_INVALID_STREAM_ID,
        "Attempt to reset the headers stream");
    return;
  }

  QuicDataStream* stream = GetDataStream(frame.stream_id);
  if (!stream) {
    // The RST frame contains the final byte offset for the stream: we can now
    // update the connection level flow controller if needed.
    UpdateFlowControlOnFinalReceivedByteOffset(frame.stream_id,
                                               frame.byte_offset);
    return;  // Errors are handled by GetStream.
  }

  stream->OnStreamReset(frame);
}

void QuicSession::OnGoAway(const QuicGoAwayFrame& frame) {
  DCHECK(frame.last_good_stream_id < next_stream_id_);
  goaway_received_ = true;
}

void QuicSession::OnConnectionClosed(QuicErrorCode error, bool from_peer) {
  DCHECK(!connection_->connected());
  if (error_ == QUIC_NO_ERROR) {
    error_ = error;
  }

  while (!stream_map_.empty()) {
    DataStreamMap::iterator it = stream_map_.begin();
    QuicStreamId id = it->first;
    it->second->OnConnectionClosed(error, from_peer);
    // The stream should call CloseStream as part of OnConnectionClosed.
    if (stream_map_.find(id) != stream_map_.end()) {
      LOG(DFATAL) << ENDPOINT
                  << "Stream failed to close under OnConnectionClosed";
      CloseStream(id);
    }
  }
}

void QuicSession::OnWindowUpdateFrames(
    const vector<QuicWindowUpdateFrame>& frames) {
  bool connection_window_updated = false;
  for (size_t i = 0; i < frames.size(); ++i) {
    // Stream may be closed by the time we receive a WINDOW_UPDATE, so we can't
    // assume that it still exists.
    QuicStreamId stream_id = frames[i].stream_id;
    if (stream_id == 0) {
      // This is a window update that applies to the connection, rather than an
      // individual stream.
      DVLOG(1) << ENDPOINT
               << "Received connection level flow control window update with "
                  "byte offset: " << frames[i].byte_offset;
      if (FLAGS_enable_quic_connection_flow_control_2 &&
          flow_controller_->UpdateSendWindowOffset(frames[i].byte_offset)) {
        connection_window_updated = true;
      }
      continue;
    }

    QuicDataStream* stream = GetDataStream(stream_id);
    if (stream) {
      stream->OnWindowUpdateFrame(frames[i]);
    }
  }

  // Connection level flow control window has increased, so blocked streams can
  // write again.
  if (connection_window_updated) {
    OnCanWrite();
  }
}

void QuicSession::OnBlockedFrames(const vector<QuicBlockedFrame>& frames) {
  for (size_t i = 0; i < frames.size(); ++i) {
    // TODO(rjshade): Compare our flow control receive windows for specified
    //                streams: if we have a large window then maybe something
    //                had gone wrong with the flow control accounting.
    DVLOG(1) << ENDPOINT << "Received BLOCKED frame with stream id: "
             << frames[i].stream_id;
  }
}

void QuicSession::OnCanWrite() {
  // We limit the number of writes to the number of pending streams. If more
  // streams become pending, WillingAndAbleToWrite will be true, which will
  // cause the connection to request resumption before yielding to other
  // connections.
  size_t num_writes = write_blocked_streams_.NumBlockedStreams();
  if (flow_controller_->IsBlocked()) {
    // If we are connection level flow control blocked, then only allow the
    // crypto and headers streams to try writing as all other streams will be
    // blocked.
    num_writes = 0;
    if (write_blocked_streams_.crypto_stream_blocked()) {
      num_writes += 1;
    }
    if (write_blocked_streams_.headers_stream_blocked()) {
      num_writes += 1;
    }
  }
  if (num_writes == 0) {
    return;
  }

  QuicConnection::ScopedPacketBundler ack_bundler(
      connection_.get(), QuicConnection::NO_ACK);
  for (size_t i = 0; i < num_writes; ++i) {
    if (!(write_blocked_streams_.HasWriteBlockedCryptoOrHeadersStream() ||
          write_blocked_streams_.HasWriteBlockedDataStreams())) {
      // Writing one stream removed another!? Something's broken.
      LOG(DFATAL) << "WriteBlockedStream is missing";
      connection_->CloseConnection(QUIC_INTERNAL_ERROR, false);
      return;
    }
    if (!connection_->CanWriteStreamData()) {
      return;
    }
    QuicStreamId stream_id = write_blocked_streams_.PopFront();
    if (stream_id == kCryptoStreamId) {
      has_pending_handshake_ = false;  // We just popped it.
    }
    ReliableQuicStream* stream = GetStream(stream_id);
    if (stream != NULL && !stream->flow_controller()->IsBlocked()) {
      // If the stream can't write all bytes, it'll re-add itself to the blocked
      // list.
      stream->OnCanWrite();
    }
  }
}

bool QuicSession::WillingAndAbleToWrite() const {
  // If the crypto or headers streams are blocked, we want to schedule a write -
  // they don't get blocked by connection level flow control. Otherwise only
  // schedule a write if we are not flow control blocked at the connection
  // level.
  return write_blocked_streams_.HasWriteBlockedCryptoOrHeadersStream() ||
         (!flow_controller_->IsBlocked() &&
          write_blocked_streams_.HasWriteBlockedDataStreams());
}

bool QuicSession::HasPendingHandshake() const {
  return has_pending_handshake_;
}

bool QuicSession::HasOpenDataStreams() const {
  return GetNumOpenStreams() > 0;
}

QuicConsumedData QuicSession::WritevData(
    QuicStreamId id,
    const IOVector& data,
    QuicStreamOffset offset,
    bool fin,
    FecProtection fec_protection,
    QuicAckNotifier::DelegateInterface* ack_notifier_delegate) {
  return connection_->SendStreamData(id, data, offset, fin, fec_protection,
                                     ack_notifier_delegate);
}

size_t QuicSession::WriteHeaders(
    QuicStreamId id,
    const SpdyHeaderBlock& headers,
    bool fin,
    QuicAckNotifier::DelegateInterface* ack_notifier_delegate) {
  return headers_stream_->WriteHeaders(id, headers, fin, ack_notifier_delegate);
}

void QuicSession::SendRstStream(QuicStreamId id,
                                QuicRstStreamErrorCode error,
                                QuicStreamOffset bytes_written) {
  if (connection()->connected()) {
    // Only send a RST_STREAM frame if still connected.
    connection_->SendRstStream(id, error, bytes_written);
  }
  CloseStreamInner(id, true);
}

void QuicSession::SendGoAway(QuicErrorCode error_code, const string& reason) {
  if (goaway_sent_) {
    return;
  }
  goaway_sent_ = true;
  connection_->SendGoAway(error_code, largest_peer_created_stream_id_, reason);
}

void QuicSession::CloseStream(QuicStreamId stream_id) {
  CloseStreamInner(stream_id, false);
}

void QuicSession::CloseStreamInner(QuicStreamId stream_id,
                                   bool locally_reset) {
  DVLOG(1) << ENDPOINT << "Closing stream " << stream_id;

  DataStreamMap::iterator it = stream_map_.find(stream_id);
  if (it == stream_map_.end()) {
    DVLOG(1) << ENDPOINT << "Stream is already closed: " << stream_id;
    return;
  }
  QuicDataStream* stream = it->second;

  // Tell the stream that a RST has been sent.
  if (locally_reset) {
    stream->set_rst_sent(true);
  }

  closed_streams_.push_back(it->second);

  // If we haven't received a FIN or RST for this stream, we need to keep track
  // of the how many bytes the stream's flow controller believes it has
  // received, for accurate connection level flow control accounting.
  if (!stream->HasFinalReceivedByteOffset() &&
      stream->flow_controller()->IsEnabled() &&
      FLAGS_enable_quic_connection_flow_control_2) {
    locally_closed_streams_highest_offset_[stream_id] =
        stream->flow_controller()->highest_received_byte_offset();
  }

  stream_map_.erase(it);
  stream->OnClose();
}

void QuicSession::UpdateFlowControlOnFinalReceivedByteOffset(
    QuicStreamId stream_id, QuicStreamOffset final_byte_offset) {
  if (!FLAGS_enable_quic_connection_flow_control_2) {
    return;
  }

  map<QuicStreamId, QuicStreamOffset>::iterator it =
      locally_closed_streams_highest_offset_.find(stream_id);
  if (it == locally_closed_streams_highest_offset_.end()) {
    return;
  }

  DVLOG(1) << ENDPOINT << "Received final byte offset " << final_byte_offset
           << " for stream " << stream_id;
  uint64 offset_diff = final_byte_offset - it->second;
  if (flow_controller_->UpdateHighestReceivedOffset(
      flow_controller_->highest_received_byte_offset() + offset_diff)) {
    // If the final offset violates flow control, close the connection now.
    if (flow_controller_->FlowControlViolation()) {
      connection_->SendConnectionClose(
          QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA);
      return;
    }
  }

  flow_controller_->AddBytesConsumed(offset_diff);
  locally_closed_streams_highest_offset_.erase(it);
}

bool QuicSession::IsEncryptionEstablished() {
  return GetCryptoStream()->encryption_established();
}

bool QuicSession::IsCryptoHandshakeConfirmed() {
  return GetCryptoStream()->handshake_confirmed();
}

void QuicSession::OnConfigNegotiated() {
  connection_->SetFromConfig(config_);
  QuicVersion version = connection()->version();
  if (version < QUIC_VERSION_17) {
    return;
  }

  if (version <= QUIC_VERSION_19) {
    // QUIC_VERSION_17,18,19 don't support independent stream/session flow
    // control windows.
    if (config_.HasReceivedInitialFlowControlWindowBytes()) {
      // Streams which were created before the SHLO was received (0-RTT
      // requests) are now informed of the peer's initial flow control window.
      uint32 new_window = config_.ReceivedInitialFlowControlWindowBytes();
      OnNewStreamFlowControlWindow(new_window);
      OnNewSessionFlowControlWindow(new_window);
    }

    return;
  }

  // QUIC_VERSION_20 and higher can have independent stream and session flow
  // control windows.
  if (config_.HasReceivedInitialStreamFlowControlWindowBytes()) {
    // Streams which were created before the SHLO was received (0-RTT
    // requests) are now informed of the peer's initial flow control window.
    OnNewStreamFlowControlWindow(
        config_.ReceivedInitialStreamFlowControlWindowBytes());
  }
  if (config_.HasReceivedInitialSessionFlowControlWindowBytes()) {
    OnNewSessionFlowControlWindow(
        config_.ReceivedInitialSessionFlowControlWindowBytes());
  }
}

void QuicSession::OnNewStreamFlowControlWindow(uint32 new_window) {
  if (new_window < kDefaultFlowControlSendWindow) {
    LOG(ERROR)
        << "Peer sent us an invalid stream flow control send window: "
        << new_window << ", below default: " << kDefaultFlowControlSendWindow;
    if (connection_->connected()) {
      connection_->SendConnectionClose(QUIC_FLOW_CONTROL_INVALID_WINDOW);
    }
    return;
  }

  for (DataStreamMap::iterator it = stream_map_.begin();
       it != stream_map_.end(); ++it) {
    it->second->flow_controller()->UpdateSendWindowOffset(new_window);
  }
}

void QuicSession::OnNewSessionFlowControlWindow(uint32 new_window) {
  if (new_window < kDefaultFlowControlSendWindow) {
    LOG(ERROR)
        << "Peer sent us an invalid session flow control send window: "
        << new_window << ", below default: " << kDefaultFlowControlSendWindow;
    if (connection_->connected()) {
      connection_->SendConnectionClose(QUIC_FLOW_CONTROL_INVALID_WINDOW);
    }
    return;
  }

  flow_controller_->UpdateSendWindowOffset(new_window);
}

void QuicSession::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  switch (event) {
    // TODO(satyamshekhar): Move the logic of setting the encrypter/decrypter
    // to QuicSession since it is the glue.
    case ENCRYPTION_FIRST_ESTABLISHED:
      break;

    case ENCRYPTION_REESTABLISHED:
      // Retransmit originally packets that were sent, since they can't be
      // decrypted by the peer.
      connection_->RetransmitUnackedPackets(INITIAL_ENCRYPTION_ONLY);
      break;

    case HANDSHAKE_CONFIRMED:
      LOG_IF(DFATAL, !config_.negotiated()) << ENDPOINT
          << "Handshake confirmed without parameter negotiation.";
      // Discard originally encrypted packets, since they can't be decrypted by
      // the peer.
      connection_->NeuterUnencryptedPackets();
      connection_->SetOverallConnectionTimeout(QuicTime::Delta::Infinite());
      max_open_streams_ = config_.max_streams_per_connection();
      break;

    default:
      LOG(ERROR) << ENDPOINT << "Got unknown handshake event: " << event;
  }
}

void QuicSession::OnCryptoHandshakeMessageSent(
    const CryptoHandshakeMessage& message) {
}

void QuicSession::OnCryptoHandshakeMessageReceived(
    const CryptoHandshakeMessage& message) {
}

QuicConfig* QuicSession::config() {
  return &config_;
}

void QuicSession::ActivateStream(QuicDataStream* stream) {
  DVLOG(1) << ENDPOINT << "num_streams: " << stream_map_.size()
           << ". activating " << stream->id();
  DCHECK_EQ(stream_map_.count(stream->id()), 0u);
  stream_map_[stream->id()] = stream;
}

QuicStreamId QuicSession::GetNextStreamId() {
  QuicStreamId id = next_stream_id_;
  next_stream_id_ += 2;
  return id;
}

ReliableQuicStream* QuicSession::GetStream(const QuicStreamId stream_id) {
  if (stream_id == kCryptoStreamId) {
    return GetCryptoStream();
  }
  if (stream_id == kHeadersStreamId) {
    return headers_stream_.get();
  }
  return GetDataStream(stream_id);
}

QuicDataStream* QuicSession::GetDataStream(const QuicStreamId stream_id) {
  if (stream_id == kCryptoStreamId) {
    DLOG(FATAL) << "Attempt to call GetDataStream with the crypto stream id";
    return NULL;
  }
  if (stream_id == kHeadersStreamId) {
    DLOG(FATAL) << "Attempt to call GetDataStream with the headers stream id";
    return NULL;
  }

  DataStreamMap::iterator it = stream_map_.find(stream_id);
  if (it != stream_map_.end()) {
    return it->second;
  }

  if (IsClosedStream(stream_id)) {
    return NULL;
  }

  if (stream_id % 2 == next_stream_id_ % 2) {
    // We've received a frame for a locally-created stream that is not
    // currently active.  This is an error.
    if (connection()->connected()) {
      connection()->SendConnectionClose(QUIC_PACKET_FOR_NONEXISTENT_STREAM);
    }
    return NULL;
  }

  return GetIncomingDataStream(stream_id);
}

QuicDataStream* QuicSession::GetIncomingDataStream(QuicStreamId stream_id) {
  if (IsClosedStream(stream_id)) {
    return NULL;
  }

  implicitly_created_streams_.erase(stream_id);
  if (stream_id > largest_peer_created_stream_id_) {
    if (stream_id - largest_peer_created_stream_id_ > kMaxStreamIdDelta) {
      // We may already have sent a connection close due to multiple reset
      // streams in the same packet.
      if (connection()->connected()) {
        LOG(ERROR) << "Trying to get stream: " << stream_id
                   << ", largest peer created stream: "
                   << largest_peer_created_stream_id_
                   << ", max delta: " << kMaxStreamIdDelta;
        connection()->SendConnectionClose(QUIC_INVALID_STREAM_ID);
      }
      return NULL;
    }
    if (largest_peer_created_stream_id_ == 0) {
      if (is_server()) {
        largest_peer_created_stream_id_= 3;
      } else {
        largest_peer_created_stream_id_= 1;
      }
    }
    for (QuicStreamId id = largest_peer_created_stream_id_ + 2;
         id < stream_id;
         id += 2) {
      implicitly_created_streams_.insert(id);
    }
    largest_peer_created_stream_id_ = stream_id;
  }
  QuicDataStream* stream = CreateIncomingDataStream(stream_id);
  if (stream == NULL) {
    return NULL;
  }
  ActivateStream(stream);
  return stream;
}

bool QuicSession::IsClosedStream(QuicStreamId id) {
  DCHECK_NE(0u, id);
  if (id == kCryptoStreamId) {
    return false;
  }
  if (id == kHeadersStreamId) {
    return false;
  }
  if (ContainsKey(stream_map_, id)) {
    // Stream is active
    return false;
  }
  if (id % 2 == next_stream_id_ % 2) {
    // Locally created streams are strictly in-order.  If the id is in the
    // range of created streams and it's not active, it must have been closed.
    return id < next_stream_id_;
  }
  // For peer created streams, we also need to consider implicitly created
  // streams.
  return id <= largest_peer_created_stream_id_ &&
      implicitly_created_streams_.count(id) == 0;
}

size_t QuicSession::GetNumOpenStreams() const {
  return stream_map_.size() + implicitly_created_streams_.size();
}

void QuicSession::MarkWriteBlocked(QuicStreamId id, QuicPriority priority) {
#ifndef NDEBUG
  ReliableQuicStream* stream = GetStream(id);
  if (stream != NULL) {
    LOG_IF(DFATAL, priority != stream->EffectivePriority())
        << ENDPOINT << "Stream " << id
        << "Priorities do not match.  Got: " << priority
        << " Expected: " << stream->EffectivePriority();
  } else {
    LOG(DFATAL) << "Marking unknown stream " << id << " blocked.";
  }
#endif

  if (id == kCryptoStreamId) {
    DCHECK(!has_pending_handshake_);
    has_pending_handshake_ = true;
    // TODO(jar): Be sure to use the highest priority for the crypto stream,
    // perhaps by adding a "special" priority for it that is higher than
    // kHighestPriority.
    priority = kHighestPriority;
  }
  write_blocked_streams_.PushBack(id, priority);
}

bool QuicSession::HasDataToWrite() const {
  return write_blocked_streams_.HasWriteBlockedCryptoOrHeadersStream() ||
         write_blocked_streams_.HasWriteBlockedDataStreams() ||
         connection_->HasQueuedData();
}

bool QuicSession::GetSSLInfo(SSLInfo* ssl_info) const {
  NOTIMPLEMENTED();
  return false;
}

void QuicSession::PostProcessAfterData() {
  STLDeleteElements(&closed_streams_);
  closed_streams_.clear();
}

void QuicSession::OnSuccessfulVersionNegotiation(const QuicVersion& version) {
  if (version < QUIC_VERSION_19) {
    flow_controller_->Disable();
  }

  // Inform all streams about the negotiated version. They may have been created
  // with a different version.
  for (DataStreamMap::iterator it = stream_map_.begin();
       it != stream_map_.end(); ++it) {
    if (version < QUIC_VERSION_17) {
      it->second->flow_controller()->Disable();
    }
  }
}

}  // namespace net
