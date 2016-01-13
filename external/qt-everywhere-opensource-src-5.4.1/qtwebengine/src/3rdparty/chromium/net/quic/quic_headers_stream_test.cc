// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_headers_stream.h"

#include "net/quic/quic_flags.h"
#include "net/quic/quic_utils.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_session_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::string;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;
using testing::_;

namespace net {
namespace test {
namespace {

class MockVisitor : public SpdyFramerVisitorInterface {
 public:
  MOCK_METHOD1(OnError, void(SpdyFramer* framer));
  MOCK_METHOD3(OnDataFrameHeader, void(SpdyStreamId stream_id,
                                       size_t length,
                                       bool fin));
  MOCK_METHOD4(OnStreamFrameData, void(SpdyStreamId stream_id,
                                       const char* data,
                                       size_t len,
                                       bool fin));
  MOCK_METHOD3(OnControlFrameHeaderData, bool(SpdyStreamId stream_id,
                                              const char* header_data,
                                              size_t len));
  MOCK_METHOD5(OnSynStream, void(SpdyStreamId stream_id,
                                 SpdyStreamId associated_stream_id,
                                 SpdyPriority priority,
                                 bool fin,
                                 bool unidirectional));
  MOCK_METHOD2(OnSynReply, void(SpdyStreamId stream_id, bool fin));
  MOCK_METHOD2(OnRstStream, void(SpdyStreamId stream_id,
                                 SpdyRstStreamStatus status));
  MOCK_METHOD1(OnSettings, void(bool clear_persisted));
  MOCK_METHOD3(OnSetting, void(SpdySettingsIds id, uint8 flags, uint32 value));
  MOCK_METHOD0(OnSettingsAck, void());
  MOCK_METHOD0(OnSettingsEnd, void());
  MOCK_METHOD2(OnPing, void(SpdyPingId unique_id, bool is_ack));
  MOCK_METHOD2(OnGoAway, void(SpdyStreamId last_accepted_stream_id,
                              SpdyGoAwayStatus status));
  MOCK_METHOD3(OnHeaders, void(SpdyStreamId stream_id, bool fin, bool end));
  MOCK_METHOD2(OnWindowUpdate, void(SpdyStreamId stream_id,
                                    uint32 delta_window_size));
  MOCK_METHOD2(OnCredentialFrameData, bool(const char* credential_data,
                                           size_t len));
  MOCK_METHOD1(OnBlocked, void(SpdyStreamId stream_id));
  MOCK_METHOD3(OnPushPromise, void(SpdyStreamId stream_id,
                                   SpdyStreamId promised_stream_id,
                                   bool end));
  MOCK_METHOD2(OnContinuation, void(SpdyStreamId stream_id, bool end));
  MOCK_METHOD6(OnAltSvc, void(SpdyStreamId stream_id,
                              uint32 max_age,
                              uint16 port,
                              StringPiece protocol_id,
                              StringPiece host,
                              StringPiece origin));
};

class QuicHeadersStreamTest : public ::testing::TestWithParam<bool> {
 public:
  static QuicVersionVector GetVersions() {
    QuicVersionVector versions;
    versions.push_back(QuicVersionMax());
    return versions;
  }

  QuicHeadersStreamTest()
      : connection_(new StrictMock<MockConnection>(is_server(), GetVersions())),
        session_(connection_),
        headers_stream_(QuicSessionPeer::GetHeadersStream(&session_)),
        body_("hello world"),
        framer_(SPDY3) {
    headers_[":version"]  = "HTTP/1.1";
    headers_[":status"] = "200 Ok";
    headers_["content-length"] = "11";
    framer_.set_visitor(&visitor_);
    EXPECT_EQ(QuicVersionMax(), session_.connection()->version());
    EXPECT_TRUE(headers_stream_ != NULL);
  }

  QuicConsumedData SaveIov(const IOVector& data) {
    const iovec* iov = data.iovec();
    int count = data.Capacity();
    for (int i = 0 ; i < count; ++i) {
      saved_data_.append(static_cast<char*>(iov[i].iov_base), iov[i].iov_len);
    }
    return QuicConsumedData(saved_data_.length(), false);
  }

  bool SaveHeaderData(const char* data, int len) {
    saved_header_data_.append(data, len);
    return true;
  }

  void SaveHeaderDataStringPiece(StringPiece data) {
    saved_header_data_.append(data.data(), data.length());
  }

  void WriteHeadersAndExpectSynStream(QuicStreamId stream_id,
                                      bool fin,
                                      QuicPriority priority) {
    WriteHeadersAndCheckData(stream_id, fin, priority, SYN_STREAM);
  }

  void WriteHeadersAndExpectSynReply(QuicStreamId stream_id,
                                     bool fin) {
    WriteHeadersAndCheckData(stream_id, fin, 0, SYN_REPLY);
  }

  void WriteHeadersAndCheckData(QuicStreamId stream_id,
                                bool fin,
                                QuicPriority priority,
                                SpdyFrameType type) {
    // Write the headers and capture the outgoing data
    EXPECT_CALL(session_, WritevData(kHeadersStreamId, _, _, false, _, NULL))
        .WillOnce(WithArgs<1>(Invoke(this, &QuicHeadersStreamTest::SaveIov)));
    headers_stream_->WriteHeaders(stream_id, headers_, fin, NULL);

    // Parse the outgoing data and check that it matches was was written.
    if (type == SYN_STREAM) {
      EXPECT_CALL(visitor_, OnSynStream(stream_id, kNoAssociatedStream, 0,
                                        // priority,
                                        fin, kNotUnidirectional));
    } else {
      EXPECT_CALL(visitor_, OnSynReply(stream_id, fin));
    }
    EXPECT_CALL(visitor_, OnControlFrameHeaderData(stream_id, _, _))
        .WillRepeatedly(WithArgs<1, 2>(
            Invoke(this, &QuicHeadersStreamTest::SaveHeaderData)));
    if (fin) {
      EXPECT_CALL(visitor_, OnStreamFrameData(stream_id, NULL, 0, true));
    }
    framer_.ProcessInput(saved_data_.data(), saved_data_.length());
    EXPECT_FALSE(framer_.HasError()) << framer_.error_code();

    CheckHeaders();
    saved_data_.clear();
  }

  void CheckHeaders() {
    SpdyHeaderBlock headers;
    EXPECT_EQ(saved_header_data_.length(),
              framer_.ParseHeaderBlockInBuffer(saved_header_data_.data(),
                                               saved_header_data_.length(),
                                               &headers));
    EXPECT_EQ(headers_, headers);
    saved_header_data_.clear();
  }

  bool is_server() {
    return GetParam();
  }

  void CloseConnection() {
    QuicConnectionPeer::CloseConnection(connection_);
  }

  static const bool kNotUnidirectional = false;
  static const bool kNoAssociatedStream = false;

  StrictMock<MockConnection>* connection_;
  StrictMock<MockSession> session_;
  QuicHeadersStream* headers_stream_;
  SpdyHeaderBlock headers_;
  string body_;
  string saved_data_;
  string saved_header_data_;
  SpdyFramer framer_;
  StrictMock<MockVisitor> visitor_;
};

INSTANTIATE_TEST_CASE_P(Tests, QuicHeadersStreamTest, testing::Bool());

TEST_P(QuicHeadersStreamTest, StreamId) {
  EXPECT_EQ(3u, headers_stream_->id());
}

TEST_P(QuicHeadersStreamTest, EffectivePriority) {
  EXPECT_EQ(0u, headers_stream_->EffectivePriority());
}

TEST_P(QuicHeadersStreamTest, WriteHeaders) {
  for (QuicStreamId stream_id = kClientDataStreamId1;
       stream_id < kClientDataStreamId3; stream_id += 2) {
    for (int count = 0; count < 2; ++count) {
      bool fin = (count == 0);
      if (is_server()) {
        WriteHeadersAndExpectSynReply(stream_id, fin);
      } else {
        for (QuicPriority priority = 0; priority < 7; ++priority) {
          WriteHeadersAndExpectSynStream(stream_id, fin, priority);
        }
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessRawData) {
  for (QuicStreamId stream_id = kClientDataStreamId1;
       stream_id < kClientDataStreamId3; stream_id += 2) {
    for (int count = 0; count < 2; ++count) {
      bool fin = (count == 0);
      for (QuicPriority priority = 0; priority < 7; ++priority) {
        // Replace with "WriteHeadersAndSaveData"
        scoped_ptr<SpdySerializedFrame> frame;
        if (is_server()) {
          SpdySynStreamIR syn_stream(stream_id);
          syn_stream.set_name_value_block(headers_);
          syn_stream.set_fin(fin);
          frame.reset(framer_.SerializeSynStream(syn_stream));
          EXPECT_CALL(session_, OnStreamHeadersPriority(stream_id, 0));
        } else {
          SpdySynReplyIR syn_reply(stream_id);
          syn_reply.set_name_value_block(headers_);
          syn_reply.set_fin(fin);
          frame.reset(framer_.SerializeSynReply(syn_reply));
        }
        EXPECT_CALL(session_, OnStreamHeaders(stream_id, _))
            .WillRepeatedly(WithArgs<1>(
                Invoke(this,
                       &QuicHeadersStreamTest::SaveHeaderDataStringPiece)));
        EXPECT_CALL(session_,
                    OnStreamHeadersComplete(stream_id, fin, frame->size()));
        headers_stream_->ProcessRawData(frame->data(), frame->size());

        CheckHeaders();
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyDataFrame) {
  SpdyDataIR data(2, "");
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(QUIC_INVALID_HEADERS_STREAM_DATA,
                                             "SPDY DATA frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  headers_stream_->ProcessRawData(frame->data(), frame->size());
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyRstStreamFrame) {
  SpdyRstStreamIR data(2, RST_STREAM_PROTOCOL_ERROR, "");
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(
                  QUIC_INVALID_HEADERS_STREAM_DATA,
                  "SPDY RST_STREAM frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  headers_stream_->ProcessRawData(frame->data(), frame->size());
}

TEST_P(QuicHeadersStreamTest, ProcessSpdySettingsFrame) {
  SpdySettingsIR data;
  data.AddSetting(SETTINGS_UPLOAD_BANDWIDTH, true, true, 0);
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(
                  QUIC_INVALID_HEADERS_STREAM_DATA,
                  "SPDY SETTINGS frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  headers_stream_->ProcessRawData(frame->data(), frame->size());
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyPingFrame) {
  SpdyPingIR data(1);
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(QUIC_INVALID_HEADERS_STREAM_DATA,
                                             "SPDY PING frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  headers_stream_->ProcessRawData(frame->data(), frame->size());
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyGoAwayFrame) {
  SpdyGoAwayIR data(1, GOAWAY_PROTOCOL_ERROR, "go away");
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(QUIC_INVALID_HEADERS_STREAM_DATA,
                                             "SPDY GOAWAY frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  headers_stream_->ProcessRawData(frame->data(), frame->size());
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyHeadersFrame) {
  SpdyHeadersIR data(1);
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(QUIC_INVALID_HEADERS_STREAM_DATA,
                                             "SPDY HEADERS frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  headers_stream_->ProcessRawData(frame->data(), frame->size());
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyWindowUpdateFrame) {
  SpdyWindowUpdateIR data(1, 1);
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(
                  QUIC_INVALID_HEADERS_STREAM_DATA,
                  "SPDY WINDOW_UPDATE frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  headers_stream_->ProcessRawData(frame->data(), frame->size());
}

TEST_P(QuicHeadersStreamTest, NoFlowControl) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control_2, true);
  EXPECT_FALSE(headers_stream_->flow_controller()->IsEnabled());
}

}  // namespace
}  // namespace test
}  // namespace net
