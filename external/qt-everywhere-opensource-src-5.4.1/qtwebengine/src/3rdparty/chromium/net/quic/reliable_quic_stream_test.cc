// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/reliable_quic_stream.h"

#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_utils.h"
#include "net/quic/quic_write_blocked_list.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/quic_config_peer.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_flow_controller_peer.h"
#include "net/quic/test_tools/quic_session_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/reliable_quic_stream_peer.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"

using base::StringPiece;
using std::min;
using testing::CreateFunctor;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::WithArgs;
using testing::_;

namespace net {
namespace test {
namespace {

const char kData1[] = "FooAndBar";
const char kData2[] = "EepAndBaz";
const size_t kDataLen = 9;
const bool kIsServer = true;
const bool kShouldProcessData = true;

class TestStream : public ReliableQuicStream {
 public:
  TestStream(QuicStreamId id,
             QuicSession* session,
             bool should_process_data)
      : ReliableQuicStream(id, session),
        should_process_data_(should_process_data) {}

  virtual uint32 ProcessRawData(const char* data, uint32 data_len) OVERRIDE {
    EXPECT_NE(0u, data_len);
    DVLOG(1) << "ProcessData data_len: " << data_len;
    data_ += string(data, data_len);
    return should_process_data_ ? data_len : 0;
  }

  virtual QuicPriority EffectivePriority() const OVERRIDE {
    return QuicUtils::HighestPriority();
  }

  using ReliableQuicStream::WriteOrBufferData;
  using ReliableQuicStream::CloseReadSide;
  using ReliableQuicStream::CloseWriteSide;
  using ReliableQuicStream::OnClose;

 private:
  bool should_process_data_;
  string data_;
};

class ReliableQuicStreamTest : public ::testing::TestWithParam<bool> {
 public:
  ReliableQuicStreamTest()
      : initial_flow_control_window_bytes_(kMaxPacketSize),
        zero_(QuicTime::Delta::Zero()),
        supported_versions_(QuicSupportedVersions()) {
    headers_[":host"] = "www.google.com";
    headers_[":path"] = "/index.hml";
    headers_[":scheme"] = "https";
    headers_["cookie"] =
        "__utma=208381060.1228362404.1372200928.1372200928.1372200928.1; "
        "__utmc=160408618; "
        "GX=DQAAAOEAAACWJYdewdE9rIrW6qw3PtVi2-d729qaa-74KqOsM1NVQblK4VhX"
        "hoALMsy6HOdDad2Sz0flUByv7etmo3mLMidGrBoljqO9hSVA40SLqpG_iuKKSHX"
        "RW3Np4bq0F0SDGDNsW0DSmTS9ufMRrlpARJDS7qAI6M3bghqJp4eABKZiRqebHT"
        "pMU-RXvTI5D5oCF1vYxYofH_l1Kviuiy3oQ1kS1enqWgbhJ2t61_SNdv-1XJIS0"
        "O3YeHLmVCs62O6zp89QwakfAWK9d3IDQvVSJzCQsvxvNIvaZFa567MawWlXg0Rh"
        "1zFMi5vzcns38-8_Sns; "
        "GA=v*2%2Fmem*57968640*47239936%2Fmem*57968640*47114716%2Fno-nm-"
        "yj*15%2Fno-cc-yj*5%2Fpc-ch*133685%2Fpc-s-cr*133947%2Fpc-s-t*1339"
        "47%2Fno-nm-yj*4%2Fno-cc-yj*1%2Fceft-as*1%2Fceft-nqas*0%2Fad-ra-c"
        "v_p%2Fad-nr-cv_p-f*1%2Fad-v-cv_p*859%2Fad-ns-cv_p-f*1%2Ffn-v-ad%"
        "2Fpc-t*250%2Fpc-cm*461%2Fpc-s-cr*722%2Fpc-s-t*722%2Fau_p*4"
        "SICAID=AJKiYcHdKgxum7KMXG0ei2t1-W4OD1uW-ecNsCqC0wDuAXiDGIcT_HA2o1"
        "3Rs1UKCuBAF9g8rWNOFbxt8PSNSHFuIhOo2t6bJAVpCsMU5Laa6lewuTMYI8MzdQP"
        "ARHKyW-koxuhMZHUnGBJAM1gJODe0cATO_KGoX4pbbFxxJ5IicRxOrWK_5rU3cdy6"
        "edlR9FsEdH6iujMcHkbE5l18ehJDwTWmBKBzVD87naobhMMrF6VvnDGxQVGp9Ir_b"
        "Rgj3RWUoPumQVCxtSOBdX0GlJOEcDTNCzQIm9BSfetog_eP_TfYubKudt5eMsXmN6"
        "QnyXHeGeK2UINUzJ-D30AFcpqYgH9_1BvYSpi7fc7_ydBU8TaD8ZRxvtnzXqj0RfG"
        "tuHghmv3aD-uzSYJ75XDdzKdizZ86IG6Fbn1XFhYZM-fbHhm3mVEXnyRW4ZuNOLFk"
        "Fas6LMcVC6Q8QLlHYbXBpdNFuGbuZGUnav5C-2I_-46lL0NGg3GewxGKGHvHEfoyn"
        "EFFlEYHsBQ98rXImL8ySDycdLEFvBPdtctPmWCfTxwmoSMLHU2SCVDhbqMWU5b0yr"
        "JBCScs_ejbKaqBDoB7ZGxTvqlrB__2ZmnHHjCr8RgMRtKNtIeuZAo ";
  }

  void set_supported_versions(const QuicVersionVector& versions) {
    supported_versions_ = versions;
  }

  void Initialize(bool stream_should_process_data) {
    connection_ =
        new StrictMock<MockConnection>(kIsServer, supported_versions_);
    session_.reset(new StrictMock<MockSession>(connection_));

    // New streams rely on having the peer's flow control receive window
    // negotiated in the config.
    QuicConfigPeer::SetReceivedInitialFlowControlWindow(
        session_->config(), initial_flow_control_window_bytes_);
    QuicConfigPeer::SetReceivedInitialStreamFlowControlWindow(
        session_->config(), initial_flow_control_window_bytes_);

    stream_.reset(new TestStream(kHeadersStreamId, session_.get(),
                                 stream_should_process_data));
    write_blocked_list_ =
        QuicSessionPeer::GetWriteBlockedStreams(session_.get());
  }

  bool fin_sent() { return ReliableQuicStreamPeer::FinSent(stream_.get()); }
  bool rst_sent() { return ReliableQuicStreamPeer::RstSent(stream_.get()); }

  void set_initial_flow_control_window_bytes(uint32 val) {
    initial_flow_control_window_bytes_ = val;
  }

  bool HasWriteBlockedStreams() {
    return write_blocked_list_->HasWriteBlockedCryptoOrHeadersStream() ||
           write_blocked_list_->HasWriteBlockedDataStreams();
  }

 protected:
  MockConnection* connection_;
  scoped_ptr<MockSession> session_;
  scoped_ptr<TestStream> stream_;
  SpdyHeaderBlock headers_;
  QuicWriteBlockedList* write_blocked_list_;
  uint32 initial_flow_control_window_bytes_;
  QuicTime::Delta zero_;
  QuicVersionVector supported_versions_;
};

TEST_F(ReliableQuicStreamTest, WriteAllData) {
  Initialize(kShouldProcessData);

  size_t length = 1 + QuicPacketCreator::StreamFramePacketOverhead(
      connection_->version(), PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
      PACKET_6BYTE_SEQUENCE_NUMBER, 0u, NOT_IN_FEC_GROUP);
  QuicConnectionPeer::GetPacketCreator(connection_)->set_max_packet_length(
      length);

  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _)).WillOnce(
      Return(QuicConsumedData(kDataLen, true)));
  stream_->WriteOrBufferData(kData1, false, NULL);
  EXPECT_FALSE(HasWriteBlockedStreams());
}

TEST_F(ReliableQuicStreamTest, NoBlockingIfNoDataOrFin) {
  Initialize(kShouldProcessData);

  // Write no data and no fin.  If we consume nothing we should not be write
  // blocked.
  EXPECT_DFATAL(stream_->WriteOrBufferData(StringPiece(), false, NULL), "");
  EXPECT_FALSE(HasWriteBlockedStreams());
}

TEST_F(ReliableQuicStreamTest, BlockIfOnlySomeDataConsumed) {
  Initialize(kShouldProcessData);

  // Write some data and no fin.  If we consume some but not all of the data,
  // we should be write blocked a not all the data was consumed.
  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(1, false)));
  stream_->WriteOrBufferData(StringPiece(kData1, 2), false, NULL);
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
}

TEST_F(ReliableQuicStreamTest, BlockIfFinNotConsumedWithData) {
  Initialize(kShouldProcessData);

  // Write some data and no fin.  If we consume all the data but not the fin,
  // we should be write blocked because the fin was not consumed.
  // (This should never actually happen as the fin should be sent out with the
  // last data)
  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(2, false)));
  stream_->WriteOrBufferData(StringPiece(kData1, 2), true, NULL);
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
}

TEST_F(ReliableQuicStreamTest, BlockIfSoloFinNotConsumed) {
  Initialize(kShouldProcessData);

  // Write no data and a fin.  If we consume nothing we should be write blocked,
  // as the fin was not consumed.
  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, false)));
  stream_->WriteOrBufferData(StringPiece(), true, NULL);
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
}

TEST_F(ReliableQuicStreamTest, WriteOrBufferData) {
  Initialize(kShouldProcessData);

  EXPECT_FALSE(HasWriteBlockedStreams());
  size_t length = 1 + QuicPacketCreator::StreamFramePacketOverhead(
      connection_->version(), PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
      PACKET_6BYTE_SEQUENCE_NUMBER, 0u, NOT_IN_FEC_GROUP);
  QuicConnectionPeer::GetPacketCreator(connection_)->set_max_packet_length(
      length);

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).WillOnce(
      Return(QuicConsumedData(kDataLen - 1, false)));
  stream_->WriteOrBufferData(kData1, false, NULL);
  EXPECT_TRUE(HasWriteBlockedStreams());

  // Queue a bytes_consumed write.
  stream_->WriteOrBufferData(kData2, false, NULL);

  // Make sure we get the tail of the first write followed by the bytes_consumed
  InSequence s;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).
      WillOnce(Return(QuicConsumedData(1, false)));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).
      WillOnce(Return(QuicConsumedData(kDataLen - 2, false)));
  stream_->OnCanWrite();

  // And finally the end of the bytes_consumed.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).
      WillOnce(Return(QuicConsumedData(2, true)));
  stream_->OnCanWrite();
}

TEST_F(ReliableQuicStreamTest, WriteOrBufferDataWithFecProtectAlways) {
  Initialize(kShouldProcessData);

  // Set FEC policy on stream.
  ReliableQuicStreamPeer::SetFecPolicy(stream_.get(), FEC_PROTECT_ALWAYS);

  EXPECT_FALSE(HasWriteBlockedStreams());
  size_t length = 1 + QuicPacketCreator::StreamFramePacketOverhead(
      connection_->version(), PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
      PACKET_6BYTE_SEQUENCE_NUMBER, 0u, IN_FEC_GROUP);
  QuicConnectionPeer::GetPacketCreator(connection_)->set_max_packet_length(
      length);

  // Write first data onto stream, which will cause one session write.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, MUST_FEC_PROTECT, _)).WillOnce(
      Return(QuicConsumedData(kDataLen - 1, false)));
  stream_->WriteOrBufferData(kData1, false, NULL);
  EXPECT_TRUE(HasWriteBlockedStreams());

  // Queue a bytes_consumed write.
  stream_->WriteOrBufferData(kData2, false, NULL);

  // Make sure we get the tail of the first write followed by the bytes_consumed
  InSequence s;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, MUST_FEC_PROTECT, _)).
      WillOnce(Return(QuicConsumedData(1, false)));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, MUST_FEC_PROTECT, _)).
      WillOnce(Return(QuicConsumedData(kDataLen - 2, false)));
  stream_->OnCanWrite();

  // And finally the end of the bytes_consumed.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, MUST_FEC_PROTECT, _)).
      WillOnce(Return(QuicConsumedData(2, true)));
  stream_->OnCanWrite();
}

TEST_F(ReliableQuicStreamTest, WriteOrBufferDataWithFecProtectOptional) {
  Initialize(kShouldProcessData);

  // Set FEC policy on stream.
  ReliableQuicStreamPeer::SetFecPolicy(stream_.get(), FEC_PROTECT_OPTIONAL);

  EXPECT_FALSE(HasWriteBlockedStreams());
  size_t length = 1 + QuicPacketCreator::StreamFramePacketOverhead(
      connection_->version(), PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
      PACKET_6BYTE_SEQUENCE_NUMBER, 0u, NOT_IN_FEC_GROUP);
  QuicConnectionPeer::GetPacketCreator(connection_)->set_max_packet_length(
      length);

  // Write first data onto stream, which will cause one session write.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, MAY_FEC_PROTECT, _)).WillOnce(
      Return(QuicConsumedData(kDataLen - 1, false)));
  stream_->WriteOrBufferData(kData1, false, NULL);
  EXPECT_TRUE(HasWriteBlockedStreams());

  // Queue a bytes_consumed write.
  stream_->WriteOrBufferData(kData2, false, NULL);

  // Make sure we get the tail of the first write followed by the bytes_consumed
  InSequence s;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, MAY_FEC_PROTECT, _)).
      WillOnce(Return(QuicConsumedData(1, false)));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, MAY_FEC_PROTECT, _)).
      WillOnce(Return(QuicConsumedData(kDataLen - 2, false)));
  stream_->OnCanWrite();

  // And finally the end of the bytes_consumed.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, MAY_FEC_PROTECT, _)).
      WillOnce(Return(QuicConsumedData(2, true)));
  stream_->OnCanWrite();
}

TEST_F(ReliableQuicStreamTest, ConnectionCloseAfterStreamClose) {
  Initialize(kShouldProcessData);

  stream_->CloseReadSide();
  stream_->CloseWriteSide();
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, stream_->connection_error());
  stream_->OnConnectionClosed(QUIC_INTERNAL_ERROR, false);
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, stream_->connection_error());
}

TEST_F(ReliableQuicStreamTest, RstAlwaysSentIfNoFinSent) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if no FIN has been sent, we send a RST.

  Initialize(kShouldProcessData);
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Write some data, with no FIN.
  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(1, false)));
  stream_->WriteOrBufferData(StringPiece(kData1, 1), false, NULL);
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Now close the stream, and expect that we send a RST.
  EXPECT_CALL(*session_, SendRstStream(_, _, _));
  stream_->OnClose();
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());
}

TEST_F(ReliableQuicStreamTest, RstNotSentIfFinSent) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if a FIN has been sent, we don't also send a RST.

  Initialize(kShouldProcessData);
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Write some data, with FIN.
  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(1, true)));
  stream_->WriteOrBufferData(StringPiece(kData1, 1), true, NULL);
  EXPECT_TRUE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Now close the stream, and expect that we do not send a RST.
  stream_->OnClose();
  EXPECT_TRUE(fin_sent());
  EXPECT_FALSE(rst_sent());
}

TEST_F(ReliableQuicStreamTest, OnlySendOneRst) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if a stream sends a RST, it doesn't send an additional RST during
  // OnClose() (this shouldn't be harmful, but we shouldn't do it anyway...)

  Initialize(kShouldProcessData);
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Reset the stream.
  const int expected_resets = 1;
  EXPECT_CALL(*session_, SendRstStream(_, _, _)).Times(expected_resets);
  stream_->Reset(QUIC_STREAM_CANCELLED);
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());

  // Now close the stream (any further resets being sent would break the
  // expectation above).
  stream_->OnClose();
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());
}

TEST_F(ReliableQuicStreamTest, StreamFlowControlMultipleWindowUpdates) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control_2, true);
  set_initial_flow_control_window_bytes(1000);

  Initialize(kShouldProcessData);

  // If we receive multiple WINDOW_UPDATES (potentially out of order), then we
  // want to make sure we latch the largest offset we see.

  // Initially should be default.
  EXPECT_EQ(
      initial_flow_control_window_bytes_,
      QuicFlowControllerPeer::SendWindowOffset(stream_->flow_controller()));

  // Check a single WINDOW_UPDATE results in correct offset.
  QuicWindowUpdateFrame window_update_1(stream_->id(), 1234);
  stream_->OnWindowUpdateFrame(window_update_1);
  EXPECT_EQ(
      window_update_1.byte_offset,
      QuicFlowControllerPeer::SendWindowOffset(stream_->flow_controller()));

  // Now send a few more WINDOW_UPDATES and make sure that only the largest is
  // remembered.
  QuicWindowUpdateFrame window_update_2(stream_->id(), 1);
  QuicWindowUpdateFrame window_update_3(stream_->id(), 9999);
  QuicWindowUpdateFrame window_update_4(stream_->id(), 5678);
  stream_->OnWindowUpdateFrame(window_update_2);
  stream_->OnWindowUpdateFrame(window_update_3);
  stream_->OnWindowUpdateFrame(window_update_4);
  EXPECT_EQ(
      window_update_3.byte_offset,
      QuicFlowControllerPeer::SendWindowOffset(stream_->flow_controller()));
}

TEST_F(ReliableQuicStreamTest, StreamFlowControlShouldNotBlockInLessThanQ017) {
  // TODO(rjshade): Remove this test when we no longer have any versions <
  //                QUIC_VERSION_17.
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control_2, true);

  // Make sure we are using a version which does not support flow control.
  QuicVersion kTestQuicVersions[] = {QUIC_VERSION_16};
  QuicVersionVector versions;
  for (size_t i = 0; i < arraysize(kTestQuicVersions); ++i) {
    versions.push_back(kTestQuicVersions[i]);
  }
  set_supported_versions(versions);

  // Peer is not talking QUIC_VERSION_17 so assumes that it can send a zero
  // length flow control receive window with no consequences.
  set_initial_flow_control_window_bytes(0);

  Initialize(kShouldProcessData);

  // The stream should _not_ be flow control blocked, because we are not talking
  // a version which has flow control enabled.
  EXPECT_FALSE(stream_->flow_controller()->IsBlocked());
}

void SaveProxyAckNotifierDelegate(
    scoped_refptr<QuicAckNotifier::DelegateInterface>* delegate_out,
    QuicAckNotifier::DelegateInterface* delegate) {
  *delegate_out = delegate;
}

TEST_F(ReliableQuicStreamTest, WriteOrBufferDataWithQuicAckNotifier) {
  Initialize(kShouldProcessData);

  scoped_refptr<MockAckNotifierDelegate> delegate(
      new StrictMock<MockAckNotifierDelegate>);

  const int kDataSize = 16 * 1024;
  const string kData(kDataSize, 'a');

  const int kFirstWriteSize = 100;
  const int kSecondWriteSize = 50;
  const int kLastWriteSize = kDataSize - kFirstWriteSize - kSecondWriteSize;

  // Set a large flow control send window so this doesn't interfere with test.
  stream_->flow_controller()->UpdateSendWindowOffset(kDataSize + 1);
  if (FLAGS_enable_quic_connection_flow_control_2) {
    session_->flow_controller()->UpdateSendWindowOffset(kDataSize + 1);
  }

  scoped_refptr<QuicAckNotifier::DelegateInterface> proxy_delegate;

  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(DoAll(WithArgs<5>(Invoke(CreateFunctor(
                          &SaveProxyAckNotifierDelegate, &proxy_delegate))),
                      Return(QuicConsumedData(kFirstWriteSize, false))));
  stream_->WriteOrBufferData(kData, false, delegate.get());
  EXPECT_TRUE(HasWriteBlockedStreams());

  EXPECT_CALL(*session_,
              WritevData(kHeadersStreamId, _, _, _, _, proxy_delegate.get()))
      .WillOnce(Return(QuicConsumedData(kSecondWriteSize, false)));
  stream_->OnCanWrite();

  // No ack expected for an empty write.
  EXPECT_CALL(*session_,
              WritevData(kHeadersStreamId, _, _, _, _, proxy_delegate.get()))
      .WillOnce(Return(QuicConsumedData(0, false)));
  stream_->OnCanWrite();

  EXPECT_CALL(*session_,
              WritevData(kHeadersStreamId, _, _, _, _, proxy_delegate.get()))
      .WillOnce(Return(QuicConsumedData(kLastWriteSize, false)));
  stream_->OnCanWrite();

  // There were two writes, so OnAckNotification is not propagated
  // until the third Ack arrives.
  proxy_delegate->OnAckNotification(1, 2, 3, 4, zero_);
  proxy_delegate->OnAckNotification(10, 20, 30, 40, zero_);

  // The arguments to delegate->OnAckNotification are the sum of the
  // arguments to proxy_delegate OnAckNotification calls.
  EXPECT_CALL(*delegate, OnAckNotification(111, 222, 333, 444, zero_));
  proxy_delegate->OnAckNotification(100, 200, 300, 400, zero_);
}

// Verify delegate behavior when packets are acked before the
// WritevData call that sends out the last byte.
TEST_F(ReliableQuicStreamTest, WriteOrBufferDataAckNotificationBeforeFlush) {
  Initialize(kShouldProcessData);

  scoped_refptr<MockAckNotifierDelegate> delegate(
      new StrictMock<MockAckNotifierDelegate>);

  const int kDataSize = 16 * 1024;
  const string kData(kDataSize, 'a');

  const int kInitialWriteSize = 100;

  // Set a large flow control send window so this doesn't interfere with test.
  stream_->flow_controller()->UpdateSendWindowOffset(kDataSize + 1);
  if (FLAGS_enable_quic_connection_flow_control_2) {
    session_->flow_controller()->UpdateSendWindowOffset(kDataSize + 1);
  }

  scoped_refptr<QuicAckNotifier::DelegateInterface> proxy_delegate;

  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(DoAll(WithArgs<5>(Invoke(CreateFunctor(
                          &SaveProxyAckNotifierDelegate, &proxy_delegate))),
                      Return(QuicConsumedData(kInitialWriteSize, false))));
  stream_->WriteOrBufferData(kData, false, delegate.get());
  EXPECT_TRUE(HasWriteBlockedStreams());

  // Handle the ack of the first write.
  proxy_delegate->OnAckNotification(1, 2, 3, 4, zero_);
  proxy_delegate = NULL;

  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _)).WillOnce(
      DoAll(WithArgs<5>(Invoke(CreateFunctor(
                &SaveProxyAckNotifierDelegate, &proxy_delegate))),
            Return(QuicConsumedData(kDataSize - kInitialWriteSize, false))));
  stream_->OnCanWrite();

  // Handle the ack for the second write.
  EXPECT_CALL(*delegate, OnAckNotification(101, 202, 303, 404, zero_));
  proxy_delegate->OnAckNotification(100, 200, 300, 400, zero_);
}

// Verify delegate behavior when WriteOrBufferData does not buffer.
TEST_F(ReliableQuicStreamTest, WriteAndBufferDataWithAckNotiferNoBuffer) {
  Initialize(kShouldProcessData);

  scoped_refptr<MockAckNotifierDelegate> delegate(
      new StrictMock<MockAckNotifierDelegate>);

  scoped_refptr<QuicAckNotifier::DelegateInterface> proxy_delegate;

  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(DoAll(WithArgs<5>(Invoke(CreateFunctor(
                          &SaveProxyAckNotifierDelegate, &proxy_delegate))),
                      Return(QuicConsumedData(kDataLen, true))));
  stream_->WriteOrBufferData(kData1, true, delegate.get());
  EXPECT_FALSE(HasWriteBlockedStreams());

  // Handle the ack.
  EXPECT_CALL(*delegate, OnAckNotification(1, 2, 3, 4, zero_));
  proxy_delegate->OnAckNotification(1, 2, 3, 4, zero_);
}

// Verify delegate behavior when WriteOrBufferData buffers all the data.
TEST_F(ReliableQuicStreamTest, BufferOnWriteAndBufferDataWithAckNotifer) {
  Initialize(kShouldProcessData);

  scoped_refptr<MockAckNotifierDelegate> delegate(
      new StrictMock<MockAckNotifierDelegate>);

  scoped_refptr<QuicAckNotifier::DelegateInterface> proxy_delegate;

  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, false)));
  stream_->WriteOrBufferData(kData1, true, delegate.get());
  EXPECT_TRUE(HasWriteBlockedStreams());

  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(DoAll(WithArgs<5>(Invoke(CreateFunctor(
                          &SaveProxyAckNotifierDelegate, &proxy_delegate))),
                      Return(QuicConsumedData(kDataLen, true))));
  stream_->OnCanWrite();

  // Handle the ack.
  EXPECT_CALL(*delegate, OnAckNotification(1, 2, 3, 4, zero_));
  proxy_delegate->OnAckNotification(1, 2, 3, 4, zero_);
}

// Verify delegate behavior when WriteOrBufferData when the FIN is
// sent out in a different packet.
TEST_F(ReliableQuicStreamTest, WriteAndBufferDataWithAckNotiferOnlyFinRemains) {
  Initialize(kShouldProcessData);

  scoped_refptr<MockAckNotifierDelegate> delegate(
      new StrictMock<MockAckNotifierDelegate>);

  scoped_refptr<QuicAckNotifier::DelegateInterface> proxy_delegate;

  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(DoAll(WithArgs<5>(Invoke(CreateFunctor(
                          &SaveProxyAckNotifierDelegate, &proxy_delegate))),
                      Return(QuicConsumedData(kDataLen, false))));
  stream_->WriteOrBufferData(kData1, true, delegate.get());
  EXPECT_TRUE(HasWriteBlockedStreams());

  EXPECT_CALL(*session_, WritevData(kHeadersStreamId, _, _, _, _, _))
      .WillOnce(DoAll(WithArgs<5>(Invoke(CreateFunctor(
                          &SaveProxyAckNotifierDelegate, &proxy_delegate))),
                      Return(QuicConsumedData(0, true))));
  stream_->OnCanWrite();

  // Handle the acks.
  proxy_delegate->OnAckNotification(1, 2, 3, 4, zero_);
  EXPECT_CALL(*delegate, OnAckNotification(11, 22, 33, 44, zero_));
  proxy_delegate->OnAckNotification(10, 20, 30, 40, zero_);
}

// Verify that when we receive a packet which violates flow control (i.e. sends
// too much data on the stream) that the stream sequencer never sees this frame,
// as we check for violation and close the connection early.
TEST_F(ReliableQuicStreamTest,
       StreamSequencerNeverSeesPacketsViolatingFlowControl) {
  ValueRestore<bool> old_stream_flag(
      &FLAGS_enable_quic_stream_flow_control_2, true);
  ValueRestore<bool> old_connection_flag(
      &FLAGS_enable_quic_connection_flow_control_2, true);

  Initialize(kShouldProcessData);

  // Receive a stream frame that violates flow control: the byte offset is
  // higher than the receive window offset.
  QuicStreamFrame frame(stream_->id(), false,
                        kInitialSessionFlowControlWindowForTest + 1,
                        MakeIOVector("."));
  EXPECT_GT(frame.offset, QuicFlowControllerPeer::ReceiveWindowOffset(
                              stream_->flow_controller()));

  // Stream should not accept the frame, and the connection should be closed.
  EXPECT_CALL(*connection_,
              SendConnectionClose(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA));
  EXPECT_FALSE(stream_->OnStreamFrame(frame));
}

TEST_F(ReliableQuicStreamTest, FinalByteOffsetFromFin) {
  Initialize(kShouldProcessData);

  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());

  QuicStreamFrame stream_frame_no_fin(stream_->id(), false, 1234,
                                      MakeIOVector("."));
  stream_->OnStreamFrame(stream_frame_no_fin);
  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());

  QuicStreamFrame stream_frame_with_fin(stream_->id(), true, 1234,
                                        MakeIOVector("."));
  stream_->OnStreamFrame(stream_frame_with_fin);
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
}

TEST_F(ReliableQuicStreamTest, FinalByteOffsetFromRst) {
  Initialize(kShouldProcessData);

  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());
  QuicRstStreamFrame rst_frame(stream_->id(), QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
}

}  // namespace
}  // namespace test
}  // namespace net
