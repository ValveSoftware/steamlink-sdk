// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_generator.h"

#include <string>

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_packet_creator_peer.h"
#include "net/quic/test_tools/quic_packet_generator_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::string;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::_;

namespace net {
namespace test {
namespace {

class MockDelegate : public QuicPacketGenerator::DelegateInterface {
 public:
  MockDelegate() {}
  virtual ~MockDelegate() OVERRIDE {}

  MOCK_METHOD3(ShouldGeneratePacket,
               bool(TransmissionType transmission_type,
                    HasRetransmittableData retransmittable,
                    IsHandshake handshake));
  MOCK_METHOD0(CreateAckFrame, QuicAckFrame*());
  MOCK_METHOD0(CreateFeedbackFrame, QuicCongestionFeedbackFrame*());
  MOCK_METHOD0(CreateStopWaitingFrame, QuicStopWaitingFrame*());
  MOCK_METHOD1(OnSerializedPacket, bool(const SerializedPacket& packet));
  MOCK_METHOD2(CloseConnection, void(QuicErrorCode, bool));

  void SetCanWriteAnything() {
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION,
                                            NO_RETRANSMITTABLE_DATA, _))
        .WillRepeatedly(Return(true));
  }

  void SetCanNotWrite() {
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION, _, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION,
                                            NO_RETRANSMITTABLE_DATA, _))
        .WillRepeatedly(Return(false));
  }

  // Use this when only ack and feedback frames should be allowed to be written.
  void SetCanWriteOnlyNonRetransmittable() {
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION, _, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION,
                                            NO_RETRANSMITTABLE_DATA, _))
        .WillRepeatedly(Return(true));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

// Simple struct for describing the contents of a packet.
// Useful in conjunction with a SimpleQuicFrame for validating
// that a packet contains the expected frames.
struct PacketContents {
  PacketContents()
      : num_ack_frames(0),
        num_connection_close_frames(0),
        num_feedback_frames(0),
        num_goaway_frames(0),
        num_rst_stream_frames(0),
        num_stop_waiting_frames(0),
        num_stream_frames(0),
        fec_group(0) {
  }

  size_t num_ack_frames;
  size_t num_connection_close_frames;
  size_t num_feedback_frames;
  size_t num_goaway_frames;
  size_t num_rst_stream_frames;
  size_t num_stop_waiting_frames;
  size_t num_stream_frames;

  QuicFecGroupNumber fec_group;
};

}  // namespace

class QuicPacketGeneratorTest : public ::testing::Test {
 protected:
  QuicPacketGeneratorTest()
      : framer_(QuicSupportedVersions(), QuicTime::Zero(), false),
        generator_(42, &framer_, &random_, &delegate_),
        creator_(QuicPacketGeneratorPeer::GetPacketCreator(&generator_)),
        packet_(0, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL),
        packet2_(0, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL),
        packet3_(0, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL),
        packet4_(0, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL),
        packet5_(0, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL),
        packet6_(0, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL),
        packet7_(0, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL) {
  }

  virtual ~QuicPacketGeneratorTest() OVERRIDE {
    delete packet_.packet;
    delete packet_.retransmittable_frames;
    delete packet2_.packet;
    delete packet2_.retransmittable_frames;
    delete packet3_.packet;
    delete packet3_.retransmittable_frames;
    delete packet4_.packet;
    delete packet4_.retransmittable_frames;
    delete packet5_.packet;
    delete packet5_.retransmittable_frames;
    delete packet6_.packet;
    delete packet6_.retransmittable_frames;
    delete packet7_.packet;
    delete packet7_.retransmittable_frames;
  }

  QuicAckFrame* CreateAckFrame() {
    // TODO(rch): Initialize this so it can be verified later.
    return new QuicAckFrame(MakeAckFrame(0, 0));
  }

  QuicCongestionFeedbackFrame* CreateFeedbackFrame() {
    QuicCongestionFeedbackFrame* frame = new QuicCongestionFeedbackFrame;
    frame->type = kFixRate;
    frame->fix_rate.bitrate = QuicBandwidth::FromBytesPerSecond(42);
    return frame;
  }

  QuicStopWaitingFrame* CreateStopWaitingFrame() {
    QuicStopWaitingFrame* frame = new QuicStopWaitingFrame();
    frame->entropy_hash = 0;
    frame->least_unacked = 0;
    return frame;
  }

  QuicRstStreamFrame* CreateRstStreamFrame() {
    return new QuicRstStreamFrame(1, QUIC_STREAM_NO_ERROR, 0);
  }

  QuicGoAwayFrame* CreateGoAwayFrame() {
    return new QuicGoAwayFrame(QUIC_NO_ERROR, 1, string());
  }

  void CheckPacketContains(const PacketContents& contents,
                           const SerializedPacket& packet) {
    size_t num_retransmittable_frames = contents.num_connection_close_frames +
        contents.num_goaway_frames + contents.num_rst_stream_frames +
        contents.num_stream_frames;
    size_t num_frames = contents.num_feedback_frames + contents.num_ack_frames +
        contents.num_stop_waiting_frames + num_retransmittable_frames;

    if (num_retransmittable_frames == 0) {
      ASSERT_TRUE(packet.retransmittable_frames == NULL);
    } else {
      ASSERT_TRUE(packet.retransmittable_frames != NULL);
      EXPECT_EQ(num_retransmittable_frames,
                packet.retransmittable_frames->frames().size());
    }

    ASSERT_TRUE(packet.packet != NULL);
    ASSERT_TRUE(simple_framer_.ProcessPacket(*packet.packet));
    EXPECT_EQ(num_frames, simple_framer_.num_frames());
    EXPECT_EQ(contents.num_ack_frames, simple_framer_.ack_frames().size());
    EXPECT_EQ(contents.num_connection_close_frames,
              simple_framer_.connection_close_frames().size());
    EXPECT_EQ(contents.num_feedback_frames,
              simple_framer_.feedback_frames().size());
    EXPECT_EQ(contents.num_goaway_frames,
              simple_framer_.goaway_frames().size());
    EXPECT_EQ(contents.num_rst_stream_frames,
              simple_framer_.rst_stream_frames().size());
    EXPECT_EQ(contents.num_stream_frames,
              simple_framer_.stream_frames().size());
    EXPECT_EQ(contents.num_stop_waiting_frames,
              simple_framer_.stop_waiting_frames().size());
    EXPECT_EQ(contents.fec_group, simple_framer_.header().fec_group);
  }

  void CheckPacketHasSingleStreamFrame(const SerializedPacket& packet) {
    ASSERT_TRUE(packet.retransmittable_frames != NULL);
    EXPECT_EQ(1u, packet.retransmittable_frames->frames().size());
    ASSERT_TRUE(packet.packet != NULL);
    ASSERT_TRUE(simple_framer_.ProcessPacket(*packet.packet));
    EXPECT_EQ(1u, simple_framer_.num_frames());
    EXPECT_EQ(1u, simple_framer_.stream_frames().size());
  }

  void CheckPacketIsFec(const SerializedPacket& packet,
                        QuicPacketSequenceNumber fec_group) {
    ASSERT_TRUE(packet.retransmittable_frames == NULL);
    ASSERT_TRUE(packet.packet != NULL);
    ASSERT_TRUE(simple_framer_.ProcessPacket(*packet.packet));
    EXPECT_TRUE(simple_framer_.header().fec_flag);
    EXPECT_EQ(fec_group, simple_framer_.fec_data().fec_group);
  }

  IOVector CreateData(size_t len) {
    data_array_.reset(new char[len]);
    memset(data_array_.get(), '?', len);
    IOVector data;
    data.Append(data_array_.get(), len);
    return data;
  }

  QuicFramer framer_;
  MockRandom random_;
  StrictMock<MockDelegate> delegate_;
  QuicPacketGenerator generator_;
  QuicPacketCreator* creator_;
  SimpleQuicFramer simple_framer_;
  SerializedPacket packet_;
  SerializedPacket packet2_;
  SerializedPacket packet3_;
  SerializedPacket packet4_;
  SerializedPacket packet5_;
  SerializedPacket packet6_;
  SerializedPacket packet7_;

 private:
  scoped_ptr<char[]> data_array_;
};

class MockDebugDelegate : public QuicPacketGenerator::DebugDelegate {
 public:
  MOCK_METHOD1(OnFrameAddedToPacket,
               void(const QuicFrame&));
};

TEST_F(QuicPacketGeneratorTest, ShouldSendAck_NotWritable) {
  delegate_.SetCanNotWrite();

  generator_.SetShouldSendAck(false, false);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, ShouldSendAck_WritableAndShouldNotFlush) {
  StrictMock<MockDebugDelegate> debug_delegate;

  generator_.set_debug_delegate(&debug_delegate);
  delegate_.SetCanWriteOnlyNonRetransmittable();
  generator_.StartBatchOperations();

  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(debug_delegate, OnFrameAddedToPacket(_)).Times(1);

  generator_.SetShouldSendAck(false, false);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, ShouldSendAck_WritableAndShouldFlush) {
  delegate_.SetCanWriteOnlyNonRetransmittable();

  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));

  generator_.SetShouldSendAck(false, false);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_ack_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest,
       ShouldSendAckWithFeedback_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteOnlyNonRetransmittable();
  generator_.StartBatchOperations();

  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(delegate_, CreateFeedbackFrame()).WillOnce(
      Return(CreateFeedbackFrame()));

  generator_.SetShouldSendAck(true, false);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest,
       ShouldSendAckWithFeedback_WritableAndShouldFlush) {
  delegate_.SetCanWriteOnlyNonRetransmittable();

  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(delegate_, CreateFeedbackFrame()).WillOnce(
      Return(CreateFeedbackFrame()));
  EXPECT_CALL(delegate_, CreateStopWaitingFrame()).WillOnce(
      Return(CreateStopWaitingFrame()));

  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));

  generator_.SetShouldSendAck(true, true);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_feedback_frames = 1;
  contents.num_stop_waiting_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_NotWritable) {
  delegate_.SetCanNotWrite();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_OnlyAckWritable) {
  delegate_.SetCanWriteOnlyNonRetransmittable();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_NotWritableBatchThenFlush) {
  delegate_.SetCanNotWrite();
  generator_.StartBatchOperations();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
  generator_.FinishBatchOperations();
  EXPECT_TRUE(generator_.HasQueuedFrames());

  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  generator_.FlushAllQueuedFrames();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_rst_stream_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_WritableAndShouldFlush) {
  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_rst_stream_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_NotWritable) {
  delegate_.SetCanNotWrite();

  QuicConsumedData consumed = generator_.ConsumeData(
      kHeadersStreamId, MakeIOVector("foo"), 2, true, MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(0u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  QuicConsumedData consumed = generator_.ConsumeData(
      kHeadersStreamId, MakeIOVector("foo"), 2, true, MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_WritableAndShouldFlush) {
  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  QuicConsumedData consumed = generator_.ConsumeData(
      kHeadersStreamId, MakeIOVector("foo"), 2, true, MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest,
       ConsumeDataMultipleTimes_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  generator_.ConsumeData(kHeadersStreamId, MakeIOVector("foo"), 2, true,
                         MAY_FEC_PROTECT, NULL);
  QuicConsumedData consumed = generator_.ConsumeData(
      3, MakeIOVector("quux"), 7, false, MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(4u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_BatchOperations) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  generator_.ConsumeData(kHeadersStreamId, MakeIOVector("foo"), 2, true,
                         MAY_FEC_PROTECT, NULL);
  QuicConsumedData consumed = generator_.ConsumeData(
      3, MakeIOVector("quux"), 7, false, MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(4u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());

  // Now both frames will be flushed out.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 2;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, ConsumeDataFEC) {
  delegate_.SetCanWriteAnything();

  // Send FEC every two packets.
  creator_->set_max_packets_per_fec_group(2);

  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet2_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet3_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet4_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet5_), Return(true)));
  }

  // Send enough data to create 3 packets: two full and one partial. Send
  // with MUST_FEC_PROTECT flag.
  size_t data_len = 2 * kDefaultMaxPacketSize + 100;
  QuicConsumedData consumed =
      generator_.ConsumeData(3, CreateData(data_len), 0, true,
                             MUST_FEC_PROTECT, NULL);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  CheckPacketHasSingleStreamFrame(packet_);
  CheckPacketHasSingleStreamFrame(packet2_);
  CheckPacketIsFec(packet3_, 1);

  CheckPacketHasSingleStreamFrame(packet4_);
  CheckPacketIsFec(packet5_, 4);
}

TEST_F(QuicPacketGeneratorTest, ConsumeDataSendsFecAtEnd) {
  delegate_.SetCanWriteAnything();

  // Enable FEC.
  creator_->set_max_packets_per_fec_group(6);
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet2_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet3_), Return(true)));
  }

  // Send enough data to create 2 packets: one full and one partial. Send
  // with MUST_FEC_PROTECT flag.
  size_t data_len = 1 * kDefaultMaxPacketSize + 100;
  QuicConsumedData consumed =
      generator_.ConsumeData(3, CreateData(data_len), 0, true,
                             MUST_FEC_PROTECT, NULL);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  CheckPacketHasSingleStreamFrame(packet_);
  CheckPacketHasSingleStreamFrame(packet2_);
  CheckPacketIsFec(packet3_, 1);
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_FramesPreviouslyQueued) {
  // Set the packet size be enough for two stream frames with 0 stream offset,
  // but not enough for a stream frame of 0 offset and one with non-zero offset.
  size_t length =
      NullEncrypter().GetCiphertextSize(0) +
      GetPacketHeaderSize(creator_->connection_id_length(),
                          true,
                          creator_->next_sequence_number_length(),
                          NOT_IN_FEC_GROUP) +
      // Add an extra 3 bytes for the payload and 1 byte so BytesFree is larger
      // than the GetMinStreamFrameSize.
      QuicFramer::GetMinStreamFrameSize(framer_.version(), 1, 0, false,
                                        NOT_IN_FEC_GROUP) + 3 +
      QuicFramer::GetMinStreamFrameSize(framer_.version(), 1, 0, true,
                                        NOT_IN_FEC_GROUP) + 1;
  creator_->set_max_packet_length(length);
  delegate_.SetCanWriteAnything();
  {
     InSequence dummy;
     EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
         DoAll(SaveArg<0>(&packet_), Return(true)));
     EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
         DoAll(SaveArg<0>(&packet2_), Return(true)));
  }
  generator_.StartBatchOperations();
  // Queue enough data to prevent a stream frame with a non-zero offset from
  // fitting.
  QuicConsumedData consumed = generator_.ConsumeData(
      kHeadersStreamId, MakeIOVector("foo"), 0, false, MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());

  // This frame will not fit with the existing frame, causing the queued frame
  // to be serialized, and it will not fit with another frame like it, so it is
  // serialized by itself.
  consumed = generator_.ConsumeData(kHeadersStreamId, MakeIOVector("bar"), 3,
                                    true, MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);
  CheckPacketContains(contents, packet2_);
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnOff) {
  delegate_.SetCanWriteAnything();
  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(creator_->IsFecProtected());

  // Send one unprotected data packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet_), Return(true)));
  QuicConsumedData consumed =
      generator_.ConsumeData(5, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                             NULL);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
  EXPECT_FALSE(creator_->IsFecProtected());
  // Verify that one data packet was sent.
  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);

  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet2_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet3_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet4_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet5_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet6_), Return(true)));
  }
  // Send enough data to create 3 packets with MUST_FEC_PROTECT flag.
  size_t data_len = 2 * kDefaultMaxPacketSize + 100;
  consumed = generator_.ConsumeData(7, CreateData(data_len), 0, true,
                                    MUST_FEC_PROTECT, NULL);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  // Verify that two FEC packets were sent.
  CheckPacketHasSingleStreamFrame(packet2_);
  CheckPacketHasSingleStreamFrame(packet3_);
  CheckPacketIsFec(packet4_, /*fec_group=*/2u);
  CheckPacketHasSingleStreamFrame(packet5_);
  CheckPacketIsFec(packet6_, /*fec_group=*/5u);  // Sent at the end of stream.

  // Send one unprotected data packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet7_), Return(true)));
  consumed = generator_.ConsumeData(7, CreateData(1u), 0, true,
                                    MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
  EXPECT_FALSE(creator_->IsFecProtected());
  // Verify that one unprotected data packet was sent.
  CheckPacketContains(contents, packet7_);
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnWithPendingFrameInCreator) {
  delegate_.SetCanWriteAnything();
  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);

  generator_.StartBatchOperations();
  // Queue enough data to prevent a stream frame with a non-zero offset from
  // fitting.
  QuicConsumedData consumed = generator_.ConsumeData(
      7, CreateData(1u), 0, true, MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  EXPECT_TRUE(creator_->HasPendingFrames());

  // Queue protected data for sending. Should cause queued frames to be flushed.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  EXPECT_FALSE(creator_->IsFecProtected());
  consumed = generator_.ConsumeData(7, CreateData(1u), 0, true,
                                    MUST_FEC_PROTECT, NULL);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 1;
  // Transmitted packet was not FEC protected.
  CheckPacketContains(contents, packet_);
  EXPECT_TRUE(creator_->IsFecProtected());
  EXPECT_TRUE(creator_->HasPendingFrames());
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnWithPendingFramesInGenerator) {
  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);

  // Queue control frames in generator.
  delegate_.SetCanNotWrite();
  generator_.SetShouldSendAck(true, true);
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  // Set up frames to write into the creator when control frames are written.
  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(delegate_, CreateFeedbackFrame()).WillOnce(
      Return(CreateFeedbackFrame()));
  EXPECT_CALL(delegate_, CreateStopWaitingFrame()).WillOnce(
      Return(CreateStopWaitingFrame()));

  // Generator should have queued control frames, and creator should be empty.
  EXPECT_TRUE(generator_.HasQueuedFrames());
  EXPECT_FALSE(creator_->HasPendingFrames());
  EXPECT_FALSE(creator_->IsFecProtected());

  // Queue protected data for sending. Should cause queued frames to be flushed.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  QuicConsumedData consumed = generator_.ConsumeData(7, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, NULL);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_feedback_frames = 1;
  contents.num_stop_waiting_frames = 1;
  CheckPacketContains(contents, packet_);

  // FEC protection should be on in creator.
  EXPECT_TRUE(creator_->IsFecProtected());
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnOffWithSubsequentFramesProtected) {
  delegate_.SetCanWriteAnything();

  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(creator_->IsFecProtected());

  // Queue stream frame to be protected in creator.
  generator_.StartBatchOperations();
  QuicConsumedData consumed = generator_.ConsumeData(5, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, NULL);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  // Creator has a pending protected frame.
  EXPECT_TRUE(creator_->HasPendingFrames());
  EXPECT_TRUE(creator_->IsFecProtected());

  // Add enough unprotected data to exceed size of current packet, so that
  // current packet is sent. Both frames will be sent out in a single packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  size_t data_len = kDefaultMaxPacketSize;
  consumed = generator_.ConsumeData(5, CreateData(data_len), 0, true,
                                    MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 2u;
  contents.fec_group = 1u;
  CheckPacketContains(contents, packet_);
  // FEC protection should still be on in creator.
  EXPECT_TRUE(creator_->IsFecProtected());
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnOffWithSubsequentPacketsProtected) {
  delegate_.SetCanWriteAnything();

  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(creator_->IsFecProtected());

  generator_.StartBatchOperations();
  // Send first packet, FEC protected.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  // Write enough data to cause a packet to be emitted.
  size_t data_len = kDefaultMaxPacketSize;
  QuicConsumedData consumed = generator_.ConsumeData(
      5, CreateData(data_len), 0, true, MUST_FEC_PROTECT, NULL);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 1u;
  contents.fec_group = 1u;
  CheckPacketContains(contents, packet_);

  // FEC should still be on in creator.
  EXPECT_TRUE(creator_->IsFecProtected());

  // Send enough unprotected data to cause second packet to be sent, which gets
  // protected because it happens to fall within an open FEC group. Data packet
  // will be followed by FEC packet.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet2_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet3_), Return(true)));
  }
  consumed = generator_.ConsumeData(5, CreateData(data_len), 0, true,
                                    MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  contents.num_stream_frames = 2u;
  CheckPacketContains(contents, packet2_);
  CheckPacketIsFec(packet3_, /*fec_group=*/1u);

  // FEC protection should be off in creator.
  EXPECT_FALSE(creator_->IsFecProtected());
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnOffThenOnWithCreatorProtectionOn) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(creator_->IsFecProtected());

  // Queue one byte of FEC protected data.
  QuicConsumedData consumed = generator_.ConsumeData(5, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, NULL);
  EXPECT_TRUE(creator_->HasPendingFrames());

  // Add more unprotected data causing first packet to be sent, FEC protected.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  size_t data_len = kDefaultMaxPacketSize;
  consumed = generator_.ConsumeData(5, CreateData(data_len), 0, true,
                                    MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 2u;
  contents.fec_group = 1u;
  CheckPacketContains(contents, packet_);

  // FEC group is still open in creator.
  EXPECT_TRUE(creator_->IsFecProtected());

  // Add data that should be protected, large enough to cause second packet to
  // be sent. Data packet should be followed by FEC packet.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet2_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet3_), Return(true)));
  }
  consumed = generator_.ConsumeData(5, CreateData(data_len), 0, true,
                                    MUST_FEC_PROTECT, NULL);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  CheckPacketContains(contents, packet2_);
  CheckPacketIsFec(packet3_, /*fec_group=*/1u);

  // FEC protection should remain on in creator.
  EXPECT_TRUE(creator_->IsFecProtected());
}

TEST_F(QuicPacketGeneratorTest, NotWritableThenBatchOperations) {
  delegate_.SetCanNotWrite();

  generator_.SetShouldSendAck(true, false);
  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());

  delegate_.SetCanWriteAnything();

  generator_.StartBatchOperations();

  // When the first write operation is invoked, the ack and feedback
  // frames will be returned.
  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(delegate_, CreateFeedbackFrame()).WillOnce(
      Return(CreateFeedbackFrame()));

  // Send some data and a control frame
  generator_.ConsumeData(3, MakeIOVector("quux"), 7, false,
                         MAY_FEC_PROTECT, NULL);
  generator_.AddControlFrame(QuicFrame(CreateGoAwayFrame()));

  // All five frames will be flushed out in a single packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_goaway_frames = 1;
  contents.num_feedback_frames = 1;
  contents.num_rst_stream_frames = 1;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, NotWritableThenBatchOperations2) {
  delegate_.SetCanNotWrite();

  generator_.SetShouldSendAck(true, false);
  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());

  delegate_.SetCanWriteAnything();

  generator_.StartBatchOperations();

  // When the first write operation is invoked, the ack and feedback
  // frames will be returned.
  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(delegate_, CreateFeedbackFrame()).WillOnce(
      Return(CreateFeedbackFrame()));

  {
    InSequence dummy;
  // All five frames will be flushed out in a single packet
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet2_), Return(true)));
  }

  // Send enough data to exceed one packet
  size_t data_len = kDefaultMaxPacketSize + 100;
  QuicConsumedData consumed =
      generator_.ConsumeData(3, CreateData(data_len), 0, true,
                             MAY_FEC_PROTECT, NULL);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  generator_.AddControlFrame(QuicFrame(CreateGoAwayFrame()));

  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  // The first packet should have the queued data and part of the stream data.
  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_feedback_frames = 1;
  contents.num_rst_stream_frames = 1;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);

  // The second should have the remainder of the stream data.
  PacketContents contents2;
  contents2.num_goaway_frames = 1;
  contents2.num_stream_frames = 1;
  CheckPacketContains(contents2, packet2_);
}

}  // namespace test
}  // namespace net
