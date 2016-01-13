// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_creator.h"

#include "base/stl_util.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_framer_peer.h"
#include "net/quic/test_tools/quic_packet_creator_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::StringPiece;
using std::ostream;
using std::string;
using std::vector;
using testing::DoAll;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::_;

namespace net {
namespace test {
namespace {

// Run tests with combinations of {QuicVersion, ToggleVersionSerialization}.
struct TestParams {
  TestParams(QuicVersion version,
             bool version_serialization)
      : version(version),
        version_serialization(version_serialization) {
  }

  friend ostream& operator<<(ostream& os, const TestParams& p) {
    os << "{ client_version: " << QuicVersionToString(p.version)
       << " include version: " << p.version_serialization << " }";
    return os;
  }

  QuicVersion version;
  bool version_serialization;
};

// Constructs various test permutations.
vector<TestParams> GetTestParams() {
  vector<TestParams> params;
  QuicVersionVector all_supported_versions = QuicSupportedVersions();
  for (size_t i = 0; i < all_supported_versions.size(); ++i) {
    params.push_back(TestParams(all_supported_versions[i], true));
    params.push_back(TestParams(all_supported_versions[i], false));
  }
  return params;
}

class QuicPacketCreatorTest : public ::testing::TestWithParam<TestParams> {
 protected:
  QuicPacketCreatorTest()
      : server_framer_(SupportedVersions(GetParam().version), QuicTime::Zero(),
                       true),
        client_framer_(SupportedVersions(GetParam().version), QuicTime::Zero(),
                       false),
        sequence_number_(0),
        connection_id_(2),
        data_("foo"),
        creator_(connection_id_, &client_framer_, &mock_random_) {
    client_framer_.set_visitor(&framer_visitor_);
    client_framer_.set_received_entropy_calculator(&entropy_calculator_);
    server_framer_.set_visitor(&framer_visitor_);
  }

  virtual ~QuicPacketCreatorTest() OVERRIDE {
  }

  void ProcessPacket(QuicPacket* packet) {
    scoped_ptr<QuicEncryptedPacket> encrypted(
        server_framer_.EncryptPacket(ENCRYPTION_NONE, sequence_number_,
                                     *packet));
    server_framer_.ProcessPacket(*encrypted);
  }

  void CheckStreamFrame(const QuicFrame& frame,
                        QuicStreamId stream_id,
                        const string& data,
                        QuicStreamOffset offset,
                        bool fin) {
    EXPECT_EQ(STREAM_FRAME, frame.type);
    ASSERT_TRUE(frame.stream_frame);
    EXPECT_EQ(stream_id, frame.stream_frame->stream_id);
    scoped_ptr<string> frame_data(frame.stream_frame->GetDataAsString());
    EXPECT_EQ(data, *frame_data);
    EXPECT_EQ(offset, frame.stream_frame->offset);
    EXPECT_EQ(fin, frame.stream_frame->fin);
  }

  // Returns the number of bytes consumed by the header of packet, including
  // the version.
  size_t GetPacketHeaderOverhead(InFecGroup is_in_fec_group) {
    return GetPacketHeaderSize(creator_.connection_id_length(),
                               kIncludeVersion,
                               creator_.next_sequence_number_length(),
                               is_in_fec_group);
  }

  // Returns the number of bytes of overhead that will be added to a packet
  // of maximum length.
  size_t GetEncryptionOverhead() {
    return creator_.max_packet_length() - client_framer_.GetMaxPlaintextSize(
        creator_.max_packet_length());
  }

  // Returns the number of bytes consumed by the non-data fields of a stream
  // frame, assuming it is the last frame in the packet
  size_t GetStreamFrameOverhead(InFecGroup is_in_fec_group) {
    return QuicFramer::GetMinStreamFrameSize(client_framer_.version(),
                                             kClientDataStreamId1, kOffset,
                                             true, is_in_fec_group);
  }

  // Enables and turns on FEC protection. Returns true if FEC protection is on.
  bool SwitchFecProtectionOn(size_t max_packets_per_fec_group) {
    creator_.set_max_packets_per_fec_group(max_packets_per_fec_group);
    creator_.StartFecProtectingPackets();
    return creator_.IsFecProtected();
  }

  static const QuicStreamOffset kOffset = 1u;

  QuicFrames frames_;
  QuicFramer server_framer_;
  QuicFramer client_framer_;
  testing::StrictMock<MockFramerVisitor> framer_visitor_;
  QuicPacketSequenceNumber sequence_number_;
  QuicConnectionId connection_id_;
  string data_;
  MockRandom mock_random_;
  QuicPacketCreator creator_;
  MockEntropyCalculator entropy_calculator_;
};

// Run all packet creator tests with all supported versions of QUIC, and with
// and without version in the packet header.
INSTANTIATE_TEST_CASE_P(QuicPacketCreatorTests,
                        QuicPacketCreatorTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(QuicPacketCreatorTest, SerializeFrames) {
  frames_.push_back(QuicFrame(new QuicAckFrame(MakeAckFrame(0u, 0u))));
  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, false, 0u, IOVector())));
  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, true, 0u, IOVector())));
  SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
  delete frames_[0].ack_frame;
  delete frames_[1].stream_frame;
  delete frames_[2].stream_frame;

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnAckFrame(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_P(QuicPacketCreatorTest, SerializeWithFEC) {
  // Enable FEC protection, and send FEC packet every 6 packets.
  EXPECT_TRUE(SwitchFecProtectionOn(6));
  // Should return false since we do not have enough packets in the FEC group to
  // trigger an FEC packet.
  ASSERT_FALSE(creator_.ShouldSendFec(/*force_close=*/false));

  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, false, 0u, IOVector())));
  SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
  delete frames_[0].stream_frame;

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecProtectedPayload(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;

  // Should return false since we do not have enough packets in the FEC group to
  // trigger an FEC packet.
  ASSERT_FALSE(creator_.ShouldSendFec(/*force_close=*/false));
  // Should return true since there are packets in the FEC group.
  ASSERT_TRUE(creator_.ShouldSendFec(/*force_close=*/true));

  serialized = creator_.SerializeFec();
  ASSERT_EQ(2u, serialized.sequence_number);
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecData(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_P(QuicPacketCreatorTest, SerializeChangingSequenceNumberLength) {
  frames_.push_back(QuicFrame(new QuicAckFrame(MakeAckFrame(0u, 0u))));
  creator_.AddSavedFrame(frames_[0]);
  creator_.set_next_sequence_number_length(PACKET_4BYTE_SEQUENCE_NUMBER);
  SerializedPacket serialized = creator_.SerializePacket();
  // The sequence number length will not change mid-packet.
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnAckFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;

  creator_.AddSavedFrame(frames_[0]);
  serialized = creator_.SerializePacket();
  // Now the actual sequence number length should have changed.
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);
  delete frames_[0].ack_frame;

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnAckFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_P(QuicPacketCreatorTest, ChangeSequenceNumberLengthMidPacket) {
  if (GetParam().version <= QUIC_VERSION_15) {
    return;
  }
  // Changing the sequence number length with queued frames in the creator
  // should hold the change until after any currently queued frames are
  // serialized.

  // Packet 1.
  // Queue a frame in the creator.
  EXPECT_FALSE(creator_.HasPendingFrames());
  QuicFrame ack_frame = QuicFrame(new QuicAckFrame(MakeAckFrame(0u, 0u)));
  creator_.AddSavedFrame(ack_frame);

  // Now change sequence number length.
  creator_.set_next_sequence_number_length(PACKET_4BYTE_SEQUENCE_NUMBER);

  // Add a STOP_WAITING frame since it contains a packet sequence number,
  // whose length should be 1.
  QuicStopWaitingFrame stop_waiting_frame;
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&stop_waiting_frame)));
  EXPECT_TRUE(creator_.HasPendingFrames());

  // Ensure the packet is successfully created.
  SerializedPacket serialized = creator_.SerializePacket();
  ASSERT_TRUE(serialized.packet);
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);

  // Verify that header in transmitted packet has 1 byte sequence length.
  QuicPacketHeader header;
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_)).WillOnce(
        DoAll(SaveArg<0>(&header), Return(true)));
    EXPECT_CALL(framer_visitor_, OnAckFrame(_));
    EXPECT_CALL(framer_visitor_, OnStopWaitingFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            header.public_header.sequence_number_length);
  delete serialized.packet;

  // Packet 2.
  EXPECT_FALSE(creator_.HasPendingFrames());
  // Generate Packet 2 with one frame -- sequence number length should now
  // change to 4 bytes.
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&stop_waiting_frame)));
  EXPECT_TRUE(creator_.HasPendingFrames());

  // Ensure the packet is successfully created.
  serialized = creator_.SerializePacket();
  ASSERT_TRUE(serialized.packet);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);

  // Verify that header in transmitted packet has 4 byte sequence length.
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_)).WillOnce(
        DoAll(SaveArg<0>(&header), Return(true)));
    EXPECT_CALL(framer_visitor_, OnStopWaitingFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            header.public_header.sequence_number_length);

  delete serialized.packet;
  delete ack_frame.ack_frame;
}

TEST_P(QuicPacketCreatorTest, SerializeWithFECChangingSequenceNumberLength) {
  // Test goal is to test the following sequence (P1 => generate Packet 1):
  // P1 <change seq num length> P2 FEC,
  // and we expect that sequence number length should not change until the end
  // of the open FEC group.

  // Enable FEC protection, and send FEC packet every 6 packets.
  EXPECT_TRUE(SwitchFecProtectionOn(6));
  // Should return false since we do not have enough packets in the FEC group to
  // trigger an FEC packet.
  ASSERT_FALSE(creator_.ShouldSendFec(/*force_close=*/false));
  frames_.push_back(QuicFrame(new QuicAckFrame(MakeAckFrame(0u, 0u))));

  // Generate Packet 1.
  creator_.AddSavedFrame(frames_[0]);
  // Change the sequence number length mid-FEC group and it should not change.
  creator_.set_next_sequence_number_length(PACKET_4BYTE_SEQUENCE_NUMBER);
  SerializedPacket serialized = creator_.SerializePacket();
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecProtectedPayload(_));
    EXPECT_CALL(framer_visitor_, OnAckFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;

  // Generate Packet 2.
  creator_.AddSavedFrame(frames_[0]);
  serialized = creator_.SerializePacket();
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecProtectedPayload(_));
    EXPECT_CALL(framer_visitor_, OnAckFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;

  // Should return false since we do not have enough packets in the FEC group to
  // trigger an FEC packet.
  ASSERT_FALSE(creator_.ShouldSendFec(/*force_close=*/false));
  // Should return true since there are packets in the FEC group.
  ASSERT_TRUE(creator_.ShouldSendFec(/*force_close=*/true));

  // Force generation of FEC packet.
  serialized = creator_.SerializeFec();
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);
  ASSERT_EQ(3u, serialized.sequence_number);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecData(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;

  // Ensure the next FEC group starts using the new sequence number length.
  serialized = creator_.SerializeAllFrames(frames_);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);
  delete frames_[0].ack_frame;
  delete serialized.packet;
}

TEST_P(QuicPacketCreatorTest, ReserializeFramesWithSequenceNumberLength) {
  // If the original packet sequence number length, the current sequence number
  // length, and the configured send sequence number length are different, the
  // retransmit must sent with the original length and the others do not change.
  creator_.set_next_sequence_number_length(PACKET_4BYTE_SEQUENCE_NUMBER);
  QuicPacketCreatorPeer::SetSequenceNumberLength(&creator_,
                                                 PACKET_2BYTE_SEQUENCE_NUMBER);
  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, false, 0u, IOVector())));
  SerializedPacket serialized =
      creator_.ReserializeAllFrames(frames_, PACKET_1BYTE_SEQUENCE_NUMBER);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            creator_.next_sequence_number_length());
  EXPECT_EQ(PACKET_2BYTE_SEQUENCE_NUMBER,
            QuicPacketCreatorPeer::GetSequenceNumberLength(&creator_));
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);
  delete frames_[0].stream_frame;

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_P(QuicPacketCreatorTest, SerializeConnectionClose) {
  QuicConnectionCloseFrame frame;
  frame.error_code = QUIC_NO_ERROR;
  frame.error_details = "error";

  SerializedPacket serialized = creator_.SerializeConnectionClose(&frame);
  ASSERT_EQ(1u, serialized.sequence_number);
  ASSERT_EQ(1u, creator_.sequence_number());

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket());
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
  EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnConnectionCloseFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_P(QuicPacketCreatorTest, SwitchFecOnOffWithNoGroup) {
  // Enable FEC protection.
  creator_.set_max_packets_per_fec_group(6);
  EXPECT_TRUE(creator_.IsFecEnabled());
  EXPECT_FALSE(creator_.IsFecProtected());

  // Turn on FEC protection.
  creator_.StartFecProtectingPackets();
  EXPECT_TRUE(creator_.IsFecProtected());
  // We have no packets in the FEC group, so no FEC packet can be created.
  EXPECT_FALSE(creator_.ShouldSendFec(/*force_close=*/true));
  // Since no packets are in FEC group yet, we should be able to turn FEC
  // off with no trouble.
  creator_.StopFecProtectingPackets();
  EXPECT_FALSE(creator_.IsFecProtected());
}

TEST_P(QuicPacketCreatorTest, SwitchFecOnOffWithGroupInProgress) {
  // Enable FEC protection, and send FEC packet every 6 packets.
  EXPECT_TRUE(SwitchFecProtectionOn(6));
  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, false, 0u, IOVector())));
  SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
  delete frames_[0].stream_frame;
  delete serialized.packet;

  EXPECT_TRUE(creator_.IsFecProtected());
  // We do not have enough packets in the FEC group to trigger an FEC packet.
  EXPECT_FALSE(creator_.ShouldSendFec(/*force_close=*/false));
  // Should return true since there are packets in the FEC group.
  EXPECT_TRUE(creator_.ShouldSendFec(/*force_close=*/true));

  // Switching FEC off should not change creator state, since there is an
  // FEC packet under construction.
  EXPECT_DFATAL(creator_.StopFecProtectingPackets(),
                "Cannot stop FEC protection with open FEC group.");
  EXPECT_TRUE(creator_.IsFecProtected());
  // Confirm that FEC packet is still under construction.
  EXPECT_TRUE(creator_.ShouldSendFec(/*force_close=*/true));

  serialized = creator_.SerializeFec();
  delete serialized.packet;

  // Switching FEC on/off should work now.
  creator_.StopFecProtectingPackets();
  EXPECT_FALSE(creator_.IsFecProtected());
  creator_.StartFecProtectingPackets();
  EXPECT_TRUE(creator_.IsFecProtected());
}

TEST_P(QuicPacketCreatorTest, SwitchFecOnWithStreamFrameQueued) {
  // Add a stream frame to the creator.
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(
      1u, MakeIOVector("test"), 0u, false, &frame);
  EXPECT_EQ(4u, consumed);
  ASSERT_TRUE(frame.stream_frame);
  EXPECT_TRUE(creator_.AddSavedFrame(frame));
  EXPECT_TRUE(creator_.HasPendingFrames());

   // Enable FEC protection, and send FEC packet every 6 packets.
  creator_.set_max_packets_per_fec_group(6);
  EXPECT_TRUE(creator_.IsFecEnabled());
  EXPECT_DFATAL(creator_.StartFecProtectingPackets(),
                "Cannot start FEC protection with pending frames.");
  EXPECT_FALSE(creator_.IsFecProtected());

  // Serialize packet for transmission.
  SerializedPacket serialized = creator_.SerializePacket();
  delete serialized.packet;
  delete serialized.retransmittable_frames;
  EXPECT_FALSE(creator_.HasPendingFrames());

  // Since all pending frames have been serialized, turning FEC on should work.
  creator_.StartFecProtectingPackets();
  EXPECT_TRUE(creator_.IsFecProtected());
}

TEST_P(QuicPacketCreatorTest, CreateStreamFrame) {
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, MakeIOVector("test"), 0u,
                                               false, &frame);
  EXPECT_EQ(4u, consumed);
  CheckStreamFrame(frame, 1u, "test", 0u, false);
  delete frame.stream_frame;
}

TEST_P(QuicPacketCreatorTest, CreateStreamFrameFin) {
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, MakeIOVector("test"), 10u,
                                               true, &frame);
  EXPECT_EQ(4u, consumed);
  CheckStreamFrame(frame, 1u, "test", 10u, true);
  delete frame.stream_frame;
}

TEST_P(QuicPacketCreatorTest, CreateStreamFrameFinOnly) {
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, IOVector(), 0u, true,
                                               &frame);
  EXPECT_EQ(0u, consumed);
  CheckStreamFrame(frame, 1u, string(), 0u, true);
  delete frame.stream_frame;
}

TEST_P(QuicPacketCreatorTest, CreateAllFreeBytesForStreamFrames) {
  const size_t overhead = GetPacketHeaderOverhead(NOT_IN_FEC_GROUP)
                          + GetEncryptionOverhead();
  for (size_t i = overhead; i < overhead + 100; ++i) {
    creator_.set_max_packet_length(i);
    const bool should_have_room = i > overhead + GetStreamFrameOverhead(
        NOT_IN_FEC_GROUP);
    ASSERT_EQ(should_have_room, creator_.HasRoomForStreamFrame(
                                    kClientDataStreamId1, kOffset));
    if (should_have_room) {
      QuicFrame frame;
      size_t bytes_consumed = creator_.CreateStreamFrame(
          kClientDataStreamId1, MakeIOVector("testdata"), kOffset, false,
          &frame);
      EXPECT_LT(0u, bytes_consumed);
      ASSERT_TRUE(creator_.AddSavedFrame(frame));
      SerializedPacket serialized_packet = creator_.SerializePacket();
      ASSERT_TRUE(serialized_packet.packet);
      delete serialized_packet.packet;
      delete serialized_packet.retransmittable_frames;
    }
  }
}

TEST_P(QuicPacketCreatorTest, StreamFrameConsumption) {
  // Compute the total overhead for a single frame in packet.
  const size_t overhead = GetPacketHeaderOverhead(NOT_IN_FEC_GROUP)
      + GetEncryptionOverhead() + GetStreamFrameOverhead(NOT_IN_FEC_GROUP);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    string data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;
    QuicFrame frame;
    size_t bytes_consumed = creator_.CreateStreamFrame(
        kClientDataStreamId1, MakeIOVector(data), kOffset, false, &frame);
    EXPECT_EQ(capacity - bytes_free, bytes_consumed);

    ASSERT_TRUE(creator_.AddSavedFrame(frame));
    // BytesFree() returns bytes available for the next frame, which will
    // be two bytes smaller since the stream frame would need to be grown.
    EXPECT_EQ(2u, creator_.ExpansionOnNewFrame());
    size_t expected_bytes_free = bytes_free < 3 ? 0 : bytes_free - 2;
    EXPECT_EQ(expected_bytes_free, creator_.BytesFree()) << "delta: " << delta;
    SerializedPacket serialized_packet = creator_.SerializePacket();
    ASSERT_TRUE(serialized_packet.packet);
    delete serialized_packet.packet;
    delete serialized_packet.retransmittable_frames;
  }
}

TEST_P(QuicPacketCreatorTest, StreamFrameConsumptionWithFec) {
  // Enable FEC protection, and send FEC packet every 6 packets.
  EXPECT_TRUE(SwitchFecProtectionOn(6));
  // Compute the total overhead for a single frame in packet.
  const size_t overhead = GetPacketHeaderOverhead(IN_FEC_GROUP)
      + GetEncryptionOverhead() + GetStreamFrameOverhead(IN_FEC_GROUP);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    string data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;
    QuicFrame frame;
    size_t bytes_consumed = creator_.CreateStreamFrame(
        kClientDataStreamId1, MakeIOVector(data), kOffset, false, &frame);
    EXPECT_EQ(capacity - bytes_free, bytes_consumed);

    ASSERT_TRUE(creator_.AddSavedFrame(frame));
    // BytesFree() returns bytes available for the next frame. Since stream
    // frame does not grow for FEC protected packets, this should be the same
    // as bytes_free (bound by 0).
    EXPECT_EQ(0u, creator_.ExpansionOnNewFrame());
    size_t expected_bytes_free = bytes_free > 0 ? bytes_free : 0;
    EXPECT_EQ(expected_bytes_free, creator_.BytesFree()) << "delta: " << delta;
    SerializedPacket serialized_packet = creator_.SerializePacket();
    ASSERT_TRUE(serialized_packet.packet);
    delete serialized_packet.packet;
    delete serialized_packet.retransmittable_frames;
  }
}

TEST_P(QuicPacketCreatorTest, CryptoStreamFramePacketPadding) {
  // Compute the total overhead for a single frame in packet.
  const size_t overhead = GetPacketHeaderOverhead(NOT_IN_FEC_GROUP)
      + GetEncryptionOverhead() + GetStreamFrameOverhead(NOT_IN_FEC_GROUP);
  ASSERT_GT(kMaxPacketSize, overhead);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    string data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;

    QuicFrame frame;
    size_t bytes_consumed = creator_.CreateStreamFrame(
        kCryptoStreamId, MakeIOVector(data), kOffset, false, &frame);
    EXPECT_LT(0u, bytes_consumed);
    ASSERT_TRUE(creator_.AddSavedFrame(frame));
    SerializedPacket serialized_packet = creator_.SerializePacket();
    ASSERT_TRUE(serialized_packet.packet);
    // If there is not enough space in the packet to fit a padding frame
    // (1 byte) and to expand the stream frame (another 2 bytes) the packet
    // will not be padded.
    if (bytes_free < 3) {
      EXPECT_EQ(client_framer_.GetMaxPlaintextSize(kDefaultMaxPacketSize)
                - bytes_free, serialized_packet.packet->length());
    } else {
      EXPECT_EQ(client_framer_.GetMaxPlaintextSize(kDefaultMaxPacketSize),
                serialized_packet.packet->length());
    }
    delete serialized_packet.packet;
    delete serialized_packet.retransmittable_frames;
  }
}

TEST_P(QuicPacketCreatorTest, NonCryptoStreamFramePacketNonPadding) {
  // Compute the total overhead for a single frame in packet.
  const size_t overhead = GetPacketHeaderOverhead(NOT_IN_FEC_GROUP)
      + GetEncryptionOverhead() + GetStreamFrameOverhead(NOT_IN_FEC_GROUP);
  ASSERT_GT(kDefaultMaxPacketSize, overhead);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    string data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;

    QuicFrame frame;
    size_t bytes_consumed = creator_.CreateStreamFrame(
        kClientDataStreamId1, MakeIOVector(data), kOffset, false, &frame);
    EXPECT_LT(0u, bytes_consumed);
    ASSERT_TRUE(creator_.AddSavedFrame(frame));
    SerializedPacket serialized_packet = creator_.SerializePacket();
    ASSERT_TRUE(serialized_packet.packet);
    if (bytes_free > 0) {
      EXPECT_EQ(client_framer_.GetMaxPlaintextSize(kDefaultMaxPacketSize)
                - bytes_free, serialized_packet.packet->length());
    } else {
      EXPECT_EQ(client_framer_.GetMaxPlaintextSize(kDefaultMaxPacketSize),
                serialized_packet.packet->length());
    }
    delete serialized_packet.packet;
    delete serialized_packet.retransmittable_frames;
  }
}

TEST_P(QuicPacketCreatorTest, SerializeVersionNegotiationPacket) {
  QuicFramerPeer::SetIsServer(&client_framer_, true);
  QuicVersionVector versions;
  versions.push_back(test::QuicVersionMax());
  scoped_ptr<QuicEncryptedPacket> encrypted(
      creator_.SerializeVersionNegotiationPacket(versions));

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnVersionNegotiationPacket(_));
  }
  QuicFramerPeer::SetIsServer(&client_framer_, false);
  client_framer_.ProcessPacket(*encrypted);
}

TEST_P(QuicPacketCreatorTest, UpdatePacketSequenceNumberLengthLeastAwaiting) {
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            creator_.next_sequence_number_length());

  creator_.set_sequence_number(64);
  creator_.UpdateSequenceNumberLength(2, 10000);
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            creator_.next_sequence_number_length());

  creator_.set_sequence_number(64 * 256);
  creator_.UpdateSequenceNumberLength(2, 10000);
  EXPECT_EQ(PACKET_2BYTE_SEQUENCE_NUMBER,
            creator_.next_sequence_number_length());

  creator_.set_sequence_number(64 * 256 * 256);
  creator_.UpdateSequenceNumberLength(2, 10000);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            creator_.next_sequence_number_length());

  creator_.set_sequence_number(GG_UINT64_C(64) * 256 * 256 * 256 * 256);
  creator_.UpdateSequenceNumberLength(2, 10000);
  EXPECT_EQ(PACKET_6BYTE_SEQUENCE_NUMBER,
            creator_.next_sequence_number_length());
}

TEST_P(QuicPacketCreatorTest, UpdatePacketSequenceNumberLengthBandwidth) {
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            creator_.next_sequence_number_length());

  creator_.UpdateSequenceNumberLength(1, 10000);
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            creator_.next_sequence_number_length());

  creator_.UpdateSequenceNumberLength(1, 10000 * 256);
  EXPECT_EQ(PACKET_2BYTE_SEQUENCE_NUMBER,
            creator_.next_sequence_number_length());

  creator_.UpdateSequenceNumberLength(1, 10000 * 256 * 256);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            creator_.next_sequence_number_length());

  creator_.UpdateSequenceNumberLength(
      1, GG_UINT64_C(1000) * 256 * 256 * 256 * 256);
  EXPECT_EQ(PACKET_6BYTE_SEQUENCE_NUMBER,
            creator_.next_sequence_number_length());
}

TEST_P(QuicPacketCreatorTest, CreateStreamFrameWithNotifier) {
  // Ensure that if CreateStreamFrame does not consume any data (e.g. a FIN only
  // frame) then any QuicAckNotifier that is passed in still gets attached to
  // the frame.
  scoped_refptr<MockAckNotifierDelegate> delegate(new MockAckNotifierDelegate);
  QuicAckNotifier notifier(delegate.get());
  QuicFrame frame;
  IOVector empty_iovector;
  bool fin = true;
  size_t consumed_bytes = creator_.CreateStreamFrameWithNotifier(
      1u, empty_iovector, 0u, fin, &notifier, &frame);
  EXPECT_EQ(0u, consumed_bytes);
  EXPECT_EQ(&notifier, frame.stream_frame->notifier);
  delete frame.stream_frame;
}

TEST_P(QuicPacketCreatorTest, SerializeFrame) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, false, 0u, IOVector())));
  SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
  delete frames_[0].stream_frame;

  QuicPacketHeader header;
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_)).WillOnce(
        DoAll(SaveArg<0>(&header), Return(true)));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  EXPECT_EQ(GetParam().version_serialization,
            header.public_header.version_flag);
  delete serialized.packet;
}

TEST_P(QuicPacketCreatorTest, CreateStreamFrameTooLarge) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  // A string larger than fits into a frame.
  size_t payload_length;
  creator_.set_max_packet_length(GetPacketLengthForOneStream(
      client_framer_.version(),
      QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
      PACKET_1BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP, &payload_length));
  QuicFrame frame;
  const string too_long_payload(payload_length * 2, 'a');
  size_t consumed = creator_.CreateStreamFrame(
      1u, MakeIOVector(too_long_payload), 0u, true, &frame);
  EXPECT_EQ(payload_length, consumed);
  const string payload(payload_length, 'a');
  CheckStreamFrame(frame, 1u, payload, 0u, false);
  delete frame.stream_frame;
}

TEST_P(QuicPacketCreatorTest, AddFrameAndSerialize) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  const size_t max_plaintext_size =
      client_framer_.GetMaxPlaintextSize(creator_.max_packet_length());
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_EQ(max_plaintext_size -
            GetPacketHeaderSize(
                creator_.connection_id_length(),
                QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
                PACKET_1BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
            creator_.BytesFree());

  // Add a variety of frame types and then a padding frame.
  QuicAckFrame ack_frame(MakeAckFrame(0u, 0u));
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&ack_frame)));
  EXPECT_TRUE(creator_.HasPendingFrames());

  QuicCongestionFeedbackFrame congestion_feedback;
  congestion_feedback.type = kFixRate;
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&congestion_feedback)));
  EXPECT_TRUE(creator_.HasPendingFrames());

  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(
      1u, MakeIOVector("test"), 0u, false, &frame);
  EXPECT_EQ(4u, consumed);
  ASSERT_TRUE(frame.stream_frame);
  EXPECT_TRUE(creator_.AddSavedFrame(frame));
  EXPECT_TRUE(creator_.HasPendingFrames());

  QuicPaddingFrame padding_frame;
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&padding_frame)));
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_EQ(0u, creator_.BytesFree());

  EXPECT_FALSE(creator_.AddSavedFrame(QuicFrame(&ack_frame)));

  // Ensure the packet is successfully created.
  SerializedPacket serialized = creator_.SerializePacket();
  ASSERT_TRUE(serialized.packet);
  delete serialized.packet;
  ASSERT_TRUE(serialized.retransmittable_frames);
  RetransmittableFrames* retransmittable = serialized.retransmittable_frames;
  ASSERT_EQ(1u, retransmittable->frames().size());
  EXPECT_EQ(STREAM_FRAME, retransmittable->frames()[0].type);
  ASSERT_TRUE(retransmittable->frames()[0].stream_frame);
  delete serialized.retransmittable_frames;

  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_EQ(max_plaintext_size -
            GetPacketHeaderSize(
                creator_.connection_id_length(),
                QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
                PACKET_1BYTE_SEQUENCE_NUMBER,
                NOT_IN_FEC_GROUP),
            creator_.BytesFree());
}

TEST_P(QuicPacketCreatorTest, SerializeTruncatedAckFrameWithLargePacketSize) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  creator_.set_max_packet_length(kMaxPacketSize);
  const size_t max_plaintext_size =
      client_framer_.GetMaxPlaintextSize(creator_.max_packet_length());

  // Serialized length of ack frame with 2000 nack ranges should be limited by
  // the number of nack ranges that can be fit in an ack frame.
  QuicAckFrame ack_frame = MakeAckFrameWithNackRanges(2000u, 0u);
  size_t frame_len = client_framer_.GetSerializedFrameLength(
      QuicFrame(&ack_frame), creator_.BytesFree(), true, true,
      NOT_IN_FEC_GROUP, PACKET_1BYTE_SEQUENCE_NUMBER);
  EXPECT_GT(creator_.BytesFree(), frame_len);
  EXPECT_GT(max_plaintext_size, creator_.PacketSize());

  // Add ack frame to creator.
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&ack_frame)));
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_GT(max_plaintext_size, creator_.PacketSize());
  EXPECT_LT(0u, creator_.BytesFree());

  // Make sure that an additional stream frame can be added to the packet.
  QuicFrame stream_frame;
  size_t consumed = creator_.CreateStreamFrame(
      2u, MakeIOVector("test"), 0u, false, &stream_frame);
  EXPECT_EQ(4u, consumed);
  ASSERT_TRUE(stream_frame.stream_frame);
  EXPECT_TRUE(creator_.AddSavedFrame(stream_frame));
  EXPECT_TRUE(creator_.HasPendingFrames());

  // Ensure the packet is successfully created, and the packet size estimate
  // matches the serialized packet length.
  EXPECT_CALL(entropy_calculator_,
             EntropyHash(_)).WillOnce(testing::Return(0));
  size_t est_packet_size = creator_.PacketSize();
  SerializedPacket serialized = creator_.SerializePacket();
  ASSERT_TRUE(serialized.packet);
  EXPECT_EQ(est_packet_size, serialized.packet->length());
  delete serialized.retransmittable_frames;
  delete serialized.packet;
}

TEST_P(QuicPacketCreatorTest, SerializeTruncatedAckFrameWithSmallPacketSize) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  creator_.set_max_packet_length(500u);

  const size_t max_plaintext_size =
      client_framer_.GetMaxPlaintextSize(creator_.max_packet_length());
  EXPECT_EQ(max_plaintext_size - creator_.PacketSize(), creator_.BytesFree());

  // Serialized length of ack frame with 2000 nack ranges should be limited by
  // the packet size.
  QuicAckFrame ack_frame = MakeAckFrameWithNackRanges(2000u, 0u);
  size_t frame_len = client_framer_.GetSerializedFrameLength(
      QuicFrame(&ack_frame), creator_.BytesFree(), true, true,
      NOT_IN_FEC_GROUP, PACKET_1BYTE_SEQUENCE_NUMBER);
  EXPECT_EQ(creator_.BytesFree(), frame_len);

  // Add ack frame to creator.
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&ack_frame)));
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_EQ(max_plaintext_size, creator_.PacketSize());
  EXPECT_EQ(0u, creator_.BytesFree());

  // Ensure the packet is successfully created, and the packet size estimate
  // may not match the serialized packet length.
  EXPECT_CALL(entropy_calculator_,
             EntropyHash(_)).WillOnce(Return(0));
  size_t est_packet_size = creator_.PacketSize();
  SerializedPacket serialized = creator_.SerializePacket();
  ASSERT_TRUE(serialized.packet);
  EXPECT_GE(est_packet_size, serialized.packet->length());
  delete serialized.packet;
}


TEST_P(QuicPacketCreatorTest, EntropyFlag) {
  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, false, 0u, IOVector())));

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 64; ++j) {
      SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
      // Verify both BoolSource and hash algorithm.
      bool expected_rand_bool =
          (mock_random_.RandUint64() & (GG_UINT64_C(1) << j)) != 0;
      bool observed_rand_bool =
          (serialized.entropy_hash & (1 << ((j+1) % 8))) != 0;
      uint8 rest_of_hash = serialized.entropy_hash & ~(1 << ((j+1) % 8));
      EXPECT_EQ(expected_rand_bool, observed_rand_bool);
      EXPECT_EQ(0, rest_of_hash);
      delete serialized.packet;
    }
    // After 64 calls, BoolSource will refresh the bucket - make sure it does.
    mock_random_.ChangeValue();
  }

  delete frames_[0].stream_frame;
}

}  // namespace
}  // namespace test
}  // namespace net
