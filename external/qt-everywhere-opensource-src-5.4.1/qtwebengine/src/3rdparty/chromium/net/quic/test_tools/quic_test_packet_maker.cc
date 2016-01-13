// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_test_packet_maker.h"

#include "net/quic/quic_framer.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"

namespace net {
namespace test {

QuicTestPacketMaker::QuicTestPacketMaker(QuicVersion version,
                                         QuicConnectionId connection_id)
    : version_(version),
      connection_id_(connection_id),
      spdy_request_framer_(SPDY3),
      spdy_response_framer_(SPDY3) {
}

QuicTestPacketMaker::~QuicTestPacketMaker() {
}

scoped_ptr<QuicEncryptedPacket> QuicTestPacketMaker::MakeRstPacket(
    QuicPacketSequenceNumber num,
    bool include_version,
    QuicStreamId stream_id,
    QuicRstStreamErrorCode error_code) {
  QuicPacketHeader header;
  header.public_header.connection_id = connection_id_;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = include_version;
  header.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
  header.packet_sequence_number = num;
  header.entropy_flag = false;
  header.fec_flag = false;
  header.fec_group = 0;

  QuicRstStreamFrame rst(stream_id, error_code, 0);
  return scoped_ptr<QuicEncryptedPacket>(MakePacket(header, QuicFrame(&rst)));
}

scoped_ptr<QuicEncryptedPacket> QuicTestPacketMaker::MakeAckAndRstPacket(
    QuicPacketSequenceNumber num,
    bool include_version,
    QuicStreamId stream_id,
    QuicRstStreamErrorCode error_code,
    QuicPacketSequenceNumber largest_received,
    QuicPacketSequenceNumber least_unacked,
    bool send_feedback) {

  QuicPacketHeader header;
  header.public_header.connection_id = connection_id_;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = include_version;
  header.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
  header.packet_sequence_number = num;
  header.entropy_flag = false;
  header.fec_flag = false;
  header.fec_group = 0;

  QuicAckFrame ack(MakeAckFrame(largest_received, least_unacked));
  ack.received_info.delta_time_largest_observed = QuicTime::Delta::Zero();
  QuicFrames frames;
  frames.push_back(QuicFrame(&ack));
  QuicCongestionFeedbackFrame feedback;
  if (send_feedback) {
    feedback.type = kTCP;
    feedback.tcp.receive_window = 256000;

    frames.push_back(QuicFrame(&feedback));
  }

  QuicStopWaitingFrame stop_waiting;
  if (version_ > QUIC_VERSION_15) {
    stop_waiting.least_unacked = least_unacked;
    frames.push_back(QuicFrame(&stop_waiting));
  }

  QuicRstStreamFrame rst(stream_id, error_code, 0);
  frames.push_back(QuicFrame(&rst));

  QuicFramer framer(SupportedVersions(version_), QuicTime::Zero(), false);
  scoped_ptr<QuicPacket> packet(
      BuildUnsizedDataPacket(&framer, header, frames).packet);
  return scoped_ptr<QuicEncryptedPacket>(framer.EncryptPacket(
      ENCRYPTION_NONE, header.packet_sequence_number, *packet));
}

scoped_ptr<QuicEncryptedPacket> QuicTestPacketMaker::MakeConnectionClosePacket(
    QuicPacketSequenceNumber num) {
  QuicPacketHeader header;
  header.public_header.connection_id = connection_id_;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
  header.packet_sequence_number = num;
  header.entropy_flag = false;
  header.fec_flag = false;
  header.fec_group = 0;

  QuicConnectionCloseFrame close;
  close.error_code = QUIC_CRYPTO_VERSION_NOT_SUPPORTED;
  close.error_details = "Time to panic!";
  return scoped_ptr<QuicEncryptedPacket>(MakePacket(header, QuicFrame(&close)));
}

scoped_ptr<QuicEncryptedPacket> QuicTestPacketMaker::MakeAckPacket(
    QuicPacketSequenceNumber sequence_number,
    QuicPacketSequenceNumber largest_received,
    QuicPacketSequenceNumber least_unacked,
    bool send_feedback) {
  QuicPacketHeader header;
  header.public_header.connection_id = connection_id_;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
  header.packet_sequence_number = sequence_number;
  header.entropy_flag = false;
  header.fec_flag = false;
  header.fec_group = 0;

  QuicAckFrame ack(MakeAckFrame(largest_received, least_unacked));
  ack.received_info.delta_time_largest_observed = QuicTime::Delta::Zero();

  QuicCongestionFeedbackFrame feedback;
  feedback.type = kTCP;
  feedback.tcp.receive_window = 256000;

  QuicFramer framer(SupportedVersions(version_), QuicTime::Zero(), false);
  QuicFrames frames;
  frames.push_back(QuicFrame(&ack));
  if (send_feedback) {
    frames.push_back(QuicFrame(&feedback));
  }

  QuicStopWaitingFrame stop_waiting;
  if (version_ > QUIC_VERSION_15) {
    stop_waiting.least_unacked = least_unacked;
    frames.push_back(QuicFrame(&stop_waiting));
  }

  scoped_ptr<QuicPacket> packet(
      BuildUnsizedDataPacket(&framer, header, frames).packet);
  return scoped_ptr<QuicEncryptedPacket>(framer.EncryptPacket(
      ENCRYPTION_NONE, header.packet_sequence_number, *packet));
}

// Returns a newly created packet to send kData on stream 1.
scoped_ptr<QuicEncryptedPacket> QuicTestPacketMaker::MakeDataPacket(
    QuicPacketSequenceNumber sequence_number,
    QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    QuicStreamOffset offset,
    base::StringPiece data) {
  InitializeHeader(sequence_number, should_include_version);
  QuicStreamFrame frame(stream_id, fin, offset, MakeIOVector(data));
  return MakePacket(header_, QuicFrame(&frame));
}

scoped_ptr<QuicEncryptedPacket> QuicTestPacketMaker::MakeRequestHeadersPacket(
    QuicPacketSequenceNumber sequence_number,
    QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    const SpdyHeaderBlock& headers) {
  InitializeHeader(sequence_number, should_include_version);
  SpdySynStreamIR syn_stream(stream_id);
  syn_stream.set_name_value_block(headers);
  syn_stream.set_fin(fin);
  syn_stream.set_priority(0);
  scoped_ptr<SpdySerializedFrame> spdy_frame(
      spdy_request_framer_.SerializeSynStream(syn_stream));
  QuicStreamFrame frame(kHeadersStreamId, false, 0,
                        MakeIOVector(base::StringPiece(spdy_frame->data(),
                                                       spdy_frame->size())));
  return MakePacket(header_, QuicFrame(&frame));
}

scoped_ptr<QuicEncryptedPacket> QuicTestPacketMaker::MakeResponseHeadersPacket(
    QuicPacketSequenceNumber sequence_number,
    QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    const SpdyHeaderBlock& headers) {
  InitializeHeader(sequence_number, should_include_version);
  SpdySynReplyIR syn_reply(stream_id);
  syn_reply.set_name_value_block(headers);
  syn_reply.set_fin(fin);
  scoped_ptr<SpdySerializedFrame> spdy_frame(
      spdy_response_framer_.SerializeSynReply(syn_reply));
  QuicStreamFrame frame(kHeadersStreamId, false, 0,
                        MakeIOVector(base::StringPiece(spdy_frame->data(),
                                                       spdy_frame->size())));
  return MakePacket(header_, QuicFrame(&frame));
}

SpdyHeaderBlock QuicTestPacketMaker::GetRequestHeaders(
    const std::string& method,
    const std::string& scheme,
    const std::string& path) {
  SpdyHeaderBlock headers;
  headers[":method"] = method;
  headers[":host"] = "www.google.com";
  headers[":path"] = path;
  headers[":scheme"] = scheme;
  headers[":version"] = "HTTP/1.1";
  return headers;
}

SpdyHeaderBlock QuicTestPacketMaker::GetResponseHeaders(
    const std::string& status) {
  SpdyHeaderBlock headers;
  headers[":status"] = status;
  headers[":version"] = "HTTP/1.1";
  headers["content-type"] = "text/plain";
  return headers;
}

scoped_ptr<QuicEncryptedPacket> QuicTestPacketMaker::MakePacket(
    const QuicPacketHeader& header,
    const QuicFrame& frame) {
  QuicFramer framer(SupportedVersions(version_), QuicTime::Zero(), false);
  QuicFrames frames;
  frames.push_back(frame);
  scoped_ptr<QuicPacket> packet(
      BuildUnsizedDataPacket(&framer, header, frames).packet);
  return scoped_ptr<QuicEncryptedPacket>(framer.EncryptPacket(
      ENCRYPTION_NONE, header.packet_sequence_number, *packet));
}

void QuicTestPacketMaker::InitializeHeader(
    QuicPacketSequenceNumber sequence_number,
    bool should_include_version) {
  header_.public_header.connection_id = connection_id_;
  header_.public_header.reset_flag = false;
  header_.public_header.version_flag = should_include_version;
  header_.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
  header_.packet_sequence_number = sequence_number;
  header_.fec_group = 0;
  header_.entropy_flag = false;
  header_.fec_flag = false;
}

}  // namespace test
}  // namespace net
