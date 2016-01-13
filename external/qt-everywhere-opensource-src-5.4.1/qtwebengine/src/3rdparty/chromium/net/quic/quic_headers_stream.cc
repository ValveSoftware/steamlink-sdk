// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_headers_stream.h"

#include "net/quic/quic_session.h"

using base::StringPiece;

namespace net {

namespace {

const QuicStreamId kInvalidStreamId = 0;

}  // namespace

// A SpdyFramer visitor which passed SYN_STREAM and SYN_REPLY frames to
// the QuicDataStream, and closes the connection if any unexpected frames
// are received.
class QuicHeadersStream::SpdyFramerVisitor
    : public SpdyFramerVisitorInterface,
      public SpdyFramerDebugVisitorInterface {
 public:
  explicit SpdyFramerVisitor(QuicHeadersStream* stream) : stream_(stream) {}

  // SpdyFramerVisitorInterface implementation
  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           bool fin,
                           bool unidirectional) OVERRIDE {
    if (!stream_->IsConnected()) {
      return;
    }

    if (associated_stream_id != 0) {
      CloseConnection("associated_stream_id != 0");
      return;
    }

    if (unidirectional != 0) {
      CloseConnection("unidirectional != 0");
      return;
    }

    stream_->OnSynStream(stream_id, priority, fin);
  }

  virtual void OnSynReply(SpdyStreamId stream_id, bool fin) OVERRIDE {
    if (!stream_->IsConnected()) {
      return;
    }

    stream_->OnSynReply(stream_id, fin);
  }

  virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) OVERRIDE {
    if (!stream_->IsConnected()) {
      return false;
    }
    stream_->OnControlFrameHeaderData(stream_id, header_data, len);
    return true;
  }

  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 bool fin) OVERRIDE {
    if (fin && len == 0) {
      // The framer invokes OnStreamFrameData with zero-length data and
      // fin = true after processing a SYN_STREAM or SYN_REPLY frame
      // that had the fin bit set.
      return;
    }
    CloseConnection("SPDY DATA frame received.");
  }

  virtual void OnError(SpdyFramer* framer) OVERRIDE {
    CloseConnection("SPDY framing error.");
  }

  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) OVERRIDE {
    CloseConnection("SPDY DATA frame received.");
  }

  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE {
    CloseConnection("SPDY RST_STREAM frame received.");
  }

  virtual void OnSetting(SpdySettingsIds id,
                         uint8 flags,
                         uint32 value) OVERRIDE {
    CloseConnection("SPDY SETTINGS frame received.");
  }

  virtual void OnSettingsAck() OVERRIDE {
    CloseConnection("SPDY SETTINGS frame received.");
  }

  virtual void OnSettingsEnd() OVERRIDE {
    CloseConnection("SPDY SETTINGS frame received.");
  }

  virtual void OnPing(SpdyPingId unique_id, bool is_ack) OVERRIDE {
    CloseConnection("SPDY PING frame received.");
  }

  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE {
    CloseConnection("SPDY GOAWAY frame received.");
  }

  virtual void OnHeaders(SpdyStreamId stream_id, bool fin, bool end) OVERRIDE {
    CloseConnection("SPDY HEADERS frame received.");
  }

  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) OVERRIDE {
    CloseConnection("SPDY WINDOW_UPDATE frame received.");
  }

  virtual void OnPushPromise(SpdyStreamId stream_id,
                             SpdyStreamId promised_stream_id,
                             bool end) OVERRIDE {
    LOG(DFATAL) << "PUSH_PROMISE frame received from a SPDY/3 framer";
    CloseConnection("SPDY PUSH_PROMISE frame received.");
  }

  virtual void OnContinuation(SpdyStreamId stream_id, bool end) OVERRIDE {
    CloseConnection("SPDY CONTINUATION frame received.");
  }

  // SpdyFramerDebugVisitorInterface implementation
  virtual void OnSendCompressedFrame(SpdyStreamId stream_id,
                                     SpdyFrameType type,
                                     size_t payload_len,
                                     size_t frame_len) OVERRIDE {}

  virtual void OnReceiveCompressedFrame(SpdyStreamId stream_id,
                                        SpdyFrameType type,
                                        size_t frame_len) OVERRIDE {
    if (stream_->IsConnected()) {
      stream_->OnCompressedFrameSize(frame_len);
    }
  }

 private:
  void CloseConnection(const string& details) {
    if (stream_->IsConnected()) {
      stream_->CloseConnectionWithDetails(
          QUIC_INVALID_HEADERS_STREAM_DATA, details);
    }
  }

 private:
  QuicHeadersStream* stream_;

  DISALLOW_COPY_AND_ASSIGN(SpdyFramerVisitor);
};

QuicHeadersStream::QuicHeadersStream(QuicSession* session)
    : ReliableQuicStream(kHeadersStreamId, session),
      stream_id_(kInvalidStreamId),
      fin_(false),
      frame_len_(0),
      spdy_framer_(SPDY3),
      spdy_framer_visitor_(new SpdyFramerVisitor(this)) {
  spdy_framer_.set_visitor(spdy_framer_visitor_.get());
  spdy_framer_.set_debug_visitor(spdy_framer_visitor_.get());
  // TODO(jri): Set headers to be always FEC protected.
  DisableFlowControl();
}

QuicHeadersStream::~QuicHeadersStream() {}

size_t QuicHeadersStream::WriteHeaders(
    QuicStreamId stream_id,
    const SpdyHeaderBlock& headers,
    bool fin,
    QuicAckNotifier::DelegateInterface* ack_notifier_delegate) {
  scoped_ptr<SpdySerializedFrame> frame;
  if (session()->is_server()) {
    SpdySynReplyIR syn_reply(stream_id);
    syn_reply.set_name_value_block(headers);
    syn_reply.set_fin(fin);
    frame.reset(spdy_framer_.SerializeFrame(syn_reply));
  } else {
    SpdySynStreamIR syn_stream(stream_id);
    syn_stream.set_name_value_block(headers);
    syn_stream.set_fin(fin);
    frame.reset(spdy_framer_.SerializeFrame(syn_stream));
  }
  WriteOrBufferData(StringPiece(frame->data(), frame->size()), false,
                    ack_notifier_delegate);
  return frame->size();
}

uint32 QuicHeadersStream::ProcessRawData(const char* data,
                                         uint32 data_len) {
  return spdy_framer_.ProcessInput(data, data_len);
}

QuicPriority QuicHeadersStream::EffectivePriority() const { return 0; }

void QuicHeadersStream::OnSynStream(SpdyStreamId stream_id,
                                    SpdyPriority priority,
                                    bool fin) {
  if (!session()->is_server()) {
    CloseConnectionWithDetails(
        QUIC_INVALID_HEADERS_STREAM_DATA,
        "SPDY SYN_STREAM frame received at the client");
    return;
  }
  DCHECK_EQ(kInvalidStreamId, stream_id_);
  stream_id_ = stream_id;
  fin_ = fin;
  session()->OnStreamHeadersPriority(stream_id, priority);
}

void QuicHeadersStream::OnSynReply(SpdyStreamId stream_id, bool fin) {
  if (session()->is_server()) {
    CloseConnectionWithDetails(
        QUIC_INVALID_HEADERS_STREAM_DATA,
        "SPDY SYN_REPLY frame received at the server");
    return;
  }
  DCHECK_EQ(kInvalidStreamId, stream_id_);
  stream_id_ = stream_id;
  fin_ = fin;
}

void QuicHeadersStream::OnControlFrameHeaderData(SpdyStreamId stream_id,
                                                 const char* header_data,
                                                 size_t len) {
  DCHECK_EQ(stream_id_, stream_id);
  if (len == 0) {
    DCHECK_NE(0u, stream_id_);
    DCHECK_NE(0u, frame_len_);
    session()->OnStreamHeadersComplete(stream_id_, fin_, frame_len_);
    // Reset state for the next frame.
    stream_id_ = kInvalidStreamId;
    fin_ = false;
    frame_len_ = 0;
  } else {
    session()->OnStreamHeaders(stream_id_, StringPiece(header_data, len));
  }
}

void QuicHeadersStream::OnCompressedFrameSize(size_t frame_len) {
  DCHECK_EQ(kInvalidStreamId, stream_id_);
  DCHECK_EQ(0u, frame_len_);
  frame_len_ = frame_len;
}

bool QuicHeadersStream::IsConnected() {
  return session()->connection()->connected();
}

}  // namespace net
