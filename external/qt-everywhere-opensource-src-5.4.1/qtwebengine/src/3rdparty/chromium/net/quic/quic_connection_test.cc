// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "net/base/net_errors.h"
#include "net/quic/congestion_control/loss_detection_interface.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_framer_peer.h"
#include "net/quic/test_tools/quic_packet_creator_peer.h"
#include "net/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::map;
using std::vector;
using testing::AnyNumber;
using testing::AtLeast;
using testing::ContainerEq;
using testing::Contains;
using testing::DoAll;
using testing::InSequence;
using testing::InvokeWithoutArgs;
using testing::Ref;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::_;

namespace net {
namespace test {
namespace {

const char data1[] = "foo";
const char data2[] = "bar";

const bool kFin = true;
const bool kEntropyFlag = true;

const QuicPacketEntropyHash kTestEntropyHash = 76;

const int kDefaultRetransmissionTimeMs = 500;
const int kMinRetransmissionTimeMs = 200;

class TestReceiveAlgorithm : public ReceiveAlgorithmInterface {
 public:
  explicit TestReceiveAlgorithm(QuicCongestionFeedbackFrame* feedback)
      : feedback_(feedback) {
  }

  bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* congestion_feedback) {
    if (feedback_ == NULL) {
      return false;
    }
    *congestion_feedback = *feedback_;
    return true;
  }

  MOCK_METHOD3(RecordIncomingPacket,
               void(QuicByteCount, QuicPacketSequenceNumber, QuicTime));

 private:
  QuicCongestionFeedbackFrame* feedback_;

  DISALLOW_COPY_AND_ASSIGN(TestReceiveAlgorithm);
};

// TaggingEncrypter appends kTagSize bytes of |tag| to the end of each message.
class TaggingEncrypter : public QuicEncrypter {
 public:
  explicit TaggingEncrypter(uint8 tag)
      : tag_(tag) {
  }

  virtual ~TaggingEncrypter() {}

  // QuicEncrypter interface.
  virtual bool SetKey(StringPiece key) OVERRIDE { return true; }
  virtual bool SetNoncePrefix(StringPiece nonce_prefix) OVERRIDE {
    return true;
  }

  virtual bool Encrypt(StringPiece nonce,
                       StringPiece associated_data,
                       StringPiece plaintext,
                       unsigned char* output) OVERRIDE {
    memcpy(output, plaintext.data(), plaintext.size());
    output += plaintext.size();
    memset(output, tag_, kTagSize);
    return true;
  }

  virtual QuicData* EncryptPacket(QuicPacketSequenceNumber sequence_number,
                                  StringPiece associated_data,
                                  StringPiece plaintext) OVERRIDE {
    const size_t len = plaintext.size() + kTagSize;
    uint8* buffer = new uint8[len];
    Encrypt(StringPiece(), associated_data, plaintext, buffer);
    return new QuicData(reinterpret_cast<char*>(buffer), len, true);
  }

  virtual size_t GetKeySize() const OVERRIDE { return 0; }
  virtual size_t GetNoncePrefixSize() const OVERRIDE { return 0; }

  virtual size_t GetMaxPlaintextSize(size_t ciphertext_size) const OVERRIDE {
    return ciphertext_size - kTagSize;
  }

  virtual size_t GetCiphertextSize(size_t plaintext_size) const OVERRIDE {
    return plaintext_size + kTagSize;
  }

  virtual StringPiece GetKey() const OVERRIDE {
    return StringPiece();
  }

  virtual StringPiece GetNoncePrefix() const OVERRIDE {
    return StringPiece();
  }

 private:
  enum {
    kTagSize = 12,
  };

  const uint8 tag_;

  DISALLOW_COPY_AND_ASSIGN(TaggingEncrypter);
};

// TaggingDecrypter ensures that the final kTagSize bytes of the message all
// have the same value and then removes them.
class TaggingDecrypter : public QuicDecrypter {
 public:
  virtual ~TaggingDecrypter() {}

  // QuicDecrypter interface
  virtual bool SetKey(StringPiece key) OVERRIDE { return true; }
  virtual bool SetNoncePrefix(StringPiece nonce_prefix) OVERRIDE {
    return true;
  }

  virtual bool Decrypt(StringPiece nonce,
                       StringPiece associated_data,
                       StringPiece ciphertext,
                       unsigned char* output,
                       size_t* output_length) OVERRIDE {
    if (ciphertext.size() < kTagSize) {
      return false;
    }
    if (!CheckTag(ciphertext, GetTag(ciphertext))) {
      return false;
    }
    *output_length = ciphertext.size() - kTagSize;
    memcpy(output, ciphertext.data(), *output_length);
    return true;
  }

  virtual QuicData* DecryptPacket(QuicPacketSequenceNumber sequence_number,
                                  StringPiece associated_data,
                                  StringPiece ciphertext) OVERRIDE {
    if (ciphertext.size() < kTagSize) {
      return NULL;
    }
    if (!CheckTag(ciphertext, GetTag(ciphertext))) {
      return NULL;
    }
    const size_t len = ciphertext.size() - kTagSize;
    uint8* buf = new uint8[len];
    memcpy(buf, ciphertext.data(), len);
    return new QuicData(reinterpret_cast<char*>(buf), len,
                        true /* owns buffer */);
  }

  virtual StringPiece GetKey() const OVERRIDE { return StringPiece(); }
  virtual StringPiece GetNoncePrefix() const OVERRIDE { return StringPiece(); }

 protected:
  virtual uint8 GetTag(StringPiece ciphertext) {
    return ciphertext.data()[ciphertext.size()-1];
  }

 private:
  enum {
    kTagSize = 12,
  };

  bool CheckTag(StringPiece ciphertext, uint8 tag) {
    for (size_t i = ciphertext.size() - kTagSize; i < ciphertext.size(); i++) {
      if (ciphertext.data()[i] != tag) {
        return false;
      }
    }

    return true;
  }
};

// StringTaggingDecrypter ensures that the final kTagSize bytes of the message
// match the expected value.
class StrictTaggingDecrypter : public TaggingDecrypter {
 public:
  explicit StrictTaggingDecrypter(uint8 tag) : tag_(tag) {}
  virtual ~StrictTaggingDecrypter() {}

  // TaggingQuicDecrypter
  virtual uint8 GetTag(StringPiece ciphertext) OVERRIDE {
    return tag_;
  }

 private:
  const uint8 tag_;
};

class TestConnectionHelper : public QuicConnectionHelperInterface {
 public:
  class TestAlarm : public QuicAlarm {
   public:
    explicit TestAlarm(QuicAlarm::Delegate* delegate)
        : QuicAlarm(delegate) {
    }

    virtual void SetImpl() OVERRIDE {}
    virtual void CancelImpl() OVERRIDE {}
    using QuicAlarm::Fire;
  };

  TestConnectionHelper(MockClock* clock, MockRandom* random_generator)
      : clock_(clock),
        random_generator_(random_generator) {
    clock_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
  }

  // QuicConnectionHelperInterface
  virtual const QuicClock* GetClock() const OVERRIDE {
    return clock_;
  }

  virtual QuicRandom* GetRandomGenerator() OVERRIDE {
    return random_generator_;
  }

  virtual QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) OVERRIDE {
    return new TestAlarm(delegate);
  }

 private:
  MockClock* clock_;
  MockRandom* random_generator_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectionHelper);
};

class TestPacketWriter : public QuicPacketWriter {
 public:
  explicit TestPacketWriter(QuicVersion version)
      : version_(version),
        framer_(SupportedVersions(version_)),
        last_packet_size_(0),
        write_blocked_(false),
        block_on_next_write_(false),
        is_write_blocked_data_buffered_(false),
        final_bytes_of_last_packet_(0),
        final_bytes_of_previous_packet_(0),
        use_tagging_decrypter_(false),
        packets_write_attempts_(0) {
  }

  // QuicPacketWriter interface
  virtual WriteResult WritePacket(
      const char* buffer, size_t buf_len,
      const IPAddressNumber& self_address,
      const IPEndPoint& peer_address) OVERRIDE {
    QuicEncryptedPacket packet(buffer, buf_len);
    ++packets_write_attempts_;

    if (packet.length() >= sizeof(final_bytes_of_last_packet_)) {
      final_bytes_of_previous_packet_ = final_bytes_of_last_packet_;
      memcpy(&final_bytes_of_last_packet_, packet.data() + packet.length() - 4,
             sizeof(final_bytes_of_last_packet_));
    }

    if (use_tagging_decrypter_) {
      framer_.framer()->SetDecrypter(new TaggingDecrypter, ENCRYPTION_NONE);
    }
    EXPECT_TRUE(framer_.ProcessPacket(packet));
    if (block_on_next_write_) {
      write_blocked_ = true;
      block_on_next_write_ = false;
    }
    if (IsWriteBlocked()) {
      return WriteResult(WRITE_STATUS_BLOCKED, -1);
    }
    last_packet_size_ = packet.length();
    return WriteResult(WRITE_STATUS_OK, last_packet_size_);
  }

  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE {
    return is_write_blocked_data_buffered_;
  }

  virtual bool IsWriteBlocked() const OVERRIDE { return write_blocked_; }

  virtual void SetWritable() OVERRIDE { write_blocked_ = false; }

  void BlockOnNextWrite() { block_on_next_write_ = true; }

  const QuicPacketHeader& header() { return framer_.header(); }

  size_t frame_count() const { return framer_.num_frames(); }

  const vector<QuicAckFrame>& ack_frames() const {
    return framer_.ack_frames();
  }

  const vector<QuicCongestionFeedbackFrame>& feedback_frames() const {
    return framer_.feedback_frames();
  }

  const vector<QuicStopWaitingFrame>& stop_waiting_frames() const {
    return framer_.stop_waiting_frames();
  }

  const vector<QuicConnectionCloseFrame>& connection_close_frames() const {
    return framer_.connection_close_frames();
  }

  const vector<QuicStreamFrame>& stream_frames() const {
    return framer_.stream_frames();
  }

  const vector<QuicPingFrame>& ping_frames() const {
    return framer_.ping_frames();
  }

  size_t last_packet_size() {
    return last_packet_size_;
  }

  const QuicVersionNegotiationPacket* version_negotiation_packet() {
    return framer_.version_negotiation_packet();
  }

  void set_is_write_blocked_data_buffered(bool buffered) {
    is_write_blocked_data_buffered_ = buffered;
  }

  void set_is_server(bool is_server) {
    // We invert is_server here, because the framer needs to parse packets
    // we send.
    QuicFramerPeer::SetIsServer(framer_.framer(), !is_server);
  }

  // final_bytes_of_last_packet_ returns the last four bytes of the previous
  // packet as a little-endian, uint32. This is intended to be used with a
  // TaggingEncrypter so that tests can determine which encrypter was used for
  // a given packet.
  uint32 final_bytes_of_last_packet() { return final_bytes_of_last_packet_; }

  // Returns the final bytes of the second to last packet.
  uint32 final_bytes_of_previous_packet() {
    return final_bytes_of_previous_packet_;
  }

  void use_tagging_decrypter() {
    use_tagging_decrypter_ = true;
  }

  uint32 packets_write_attempts() { return packets_write_attempts_; }

  void Reset() { framer_.Reset(); }

  void SetSupportedVersions(const QuicVersionVector& versions) {
    framer_.SetSupportedVersions(versions);
  }

 private:
  QuicVersion version_;
  SimpleQuicFramer framer_;
  size_t last_packet_size_;
  bool write_blocked_;
  bool block_on_next_write_;
  bool is_write_blocked_data_buffered_;
  uint32 final_bytes_of_last_packet_;
  uint32 final_bytes_of_previous_packet_;
  bool use_tagging_decrypter_;
  uint32 packets_write_attempts_;

  DISALLOW_COPY_AND_ASSIGN(TestPacketWriter);
};

class TestConnection : public QuicConnection {
 public:
  TestConnection(QuicConnectionId connection_id,
                 IPEndPoint address,
                 TestConnectionHelper* helper,
                 TestPacketWriter* writer,
                 bool is_server,
                 QuicVersion version)
      : QuicConnection(connection_id, address, helper, writer, is_server,
                       SupportedVersions(version)),
        writer_(writer) {
    // Disable tail loss probes for most tests.
    QuicSentPacketManagerPeer::SetMaxTailLossProbes(
        QuicConnectionPeer::GetSentPacketManager(this), 0);
    writer_->set_is_server(is_server);
  }

  void SendAck() {
    QuicConnectionPeer::SendAck(this);
  }

  void SetReceiveAlgorithm(TestReceiveAlgorithm* receive_algorithm) {
     QuicConnectionPeer::SetReceiveAlgorithm(this, receive_algorithm);
  }

  void SetSendAlgorithm(SendAlgorithmInterface* send_algorithm) {
    QuicConnectionPeer::SetSendAlgorithm(this, send_algorithm);
  }

  void SetLossAlgorithm(LossDetectionInterface* loss_algorithm) {
    QuicSentPacketManagerPeer::SetLossAlgorithm(
        QuicConnectionPeer::GetSentPacketManager(this), loss_algorithm);
  }

  void SendPacket(EncryptionLevel level,
                  QuicPacketSequenceNumber sequence_number,
                  QuicPacket* packet,
                  QuicPacketEntropyHash entropy_hash,
                  HasRetransmittableData retransmittable) {
    RetransmittableFrames* retransmittable_frames =
        retransmittable == HAS_RETRANSMITTABLE_DATA ?
            new RetransmittableFrames() : NULL;
    OnSerializedPacket(
        SerializedPacket(sequence_number, PACKET_6BYTE_SEQUENCE_NUMBER,
                         packet, entropy_hash, retransmittable_frames));
  }

  QuicConsumedData SendStreamDataWithString(
      QuicStreamId id,
      StringPiece data,
      QuicStreamOffset offset,
      bool fin,
      QuicAckNotifier::DelegateInterface* delegate) {
    return SendStreamDataWithStringHelper(id, data, offset, fin,
                                          MAY_FEC_PROTECT, delegate);
  }

  QuicConsumedData SendStreamDataWithStringWithFec(
      QuicStreamId id,
      StringPiece data,
      QuicStreamOffset offset,
      bool fin,
      QuicAckNotifier::DelegateInterface* delegate) {
    return SendStreamDataWithStringHelper(id, data, offset, fin,
                                          MUST_FEC_PROTECT, delegate);
  }

  QuicConsumedData SendStreamDataWithStringHelper(
      QuicStreamId id,
      StringPiece data,
      QuicStreamOffset offset,
      bool fin,
      FecProtection fec_protection,
      QuicAckNotifier::DelegateInterface* delegate) {
    IOVector data_iov;
    if (!data.empty()) {
      data_iov.Append(const_cast<char*>(data.data()), data.size());
    }
    return QuicConnection::SendStreamData(id, data_iov, offset, fin,
                                          fec_protection, delegate);
  }

  QuicConsumedData SendStreamData3() {
    return SendStreamDataWithString(kClientDataStreamId1, "food", 0, !kFin,
                                    NULL);
  }

  QuicConsumedData SendStreamData3WithFec() {
    return SendStreamDataWithStringWithFec(kClientDataStreamId1, "food", 0,
                                           !kFin, NULL);
  }

  QuicConsumedData SendStreamData5() {
    return SendStreamDataWithString(kClientDataStreamId2, "food2", 0,
                                    !kFin, NULL);
  }

  QuicConsumedData SendStreamData5WithFec() {
    return SendStreamDataWithStringWithFec(kClientDataStreamId2, "food2", 0,
                                           !kFin, NULL);
  }
  // Ensures the connection can write stream data before writing.
  QuicConsumedData EnsureWritableAndSendStreamData5() {
    EXPECT_TRUE(CanWriteStreamData());
    return SendStreamData5();
  }

  // The crypto stream has special semantics so that it is not blocked by a
  // congestion window limitation, and also so that it gets put into a separate
  // packet (so that it is easier to reason about a crypto frame not being
  // split needlessly across packet boundaries).  As a result, we have separate
  // tests for some cases for this stream.
  QuicConsumedData SendCryptoStreamData() {
    return SendStreamDataWithString(kCryptoStreamId, "chlo", 0, !kFin, NULL);
  }

  bool is_server() {
    return QuicConnectionPeer::IsServer(this);
  }

  void set_version(QuicVersion version) {
    QuicConnectionPeer::GetFramer(this)->set_version(version);
  }

  void SetSupportedVersions(const QuicVersionVector& versions) {
    QuicConnectionPeer::GetFramer(this)->SetSupportedVersions(versions);
    writer_->SetSupportedVersions(versions);
  }

  void set_is_server(bool is_server) {
    writer_->set_is_server(is_server);
    QuicConnectionPeer::SetIsServer(this, is_server);
  }

  TestConnectionHelper::TestAlarm* GetAckAlarm() {
    return reinterpret_cast<TestConnectionHelper::TestAlarm*>(
        QuicConnectionPeer::GetAckAlarm(this));
  }

  TestConnectionHelper::TestAlarm* GetPingAlarm() {
    return reinterpret_cast<TestConnectionHelper::TestAlarm*>(
        QuicConnectionPeer::GetPingAlarm(this));
  }

  TestConnectionHelper::TestAlarm* GetResumeWritesAlarm() {
    return reinterpret_cast<TestConnectionHelper::TestAlarm*>(
        QuicConnectionPeer::GetResumeWritesAlarm(this));
  }

  TestConnectionHelper::TestAlarm* GetRetransmissionAlarm() {
    return reinterpret_cast<TestConnectionHelper::TestAlarm*>(
        QuicConnectionPeer::GetRetransmissionAlarm(this));
  }

  TestConnectionHelper::TestAlarm* GetSendAlarm() {
    return reinterpret_cast<TestConnectionHelper::TestAlarm*>(
        QuicConnectionPeer::GetSendAlarm(this));
  }

  TestConnectionHelper::TestAlarm* GetTimeoutAlarm() {
    return reinterpret_cast<TestConnectionHelper::TestAlarm*>(
        QuicConnectionPeer::GetTimeoutAlarm(this));
  }

  using QuicConnection::SelectMutualVersion;

 private:
  TestPacketWriter* writer_;

  DISALLOW_COPY_AND_ASSIGN(TestConnection);
};

// Used for testing packets revived from FEC packets.
class FecQuicConnectionDebugVisitor
    : public QuicConnectionDebugVisitor {
 public:
  virtual void OnRevivedPacket(const QuicPacketHeader& header,
                               StringPiece data) OVERRIDE {
    revived_header_ = header;
  }

  // Public accessor method.
  QuicPacketHeader revived_header() const {
    return revived_header_;
  }

 private:
  QuicPacketHeader revived_header_;
};

class QuicConnectionTest : public ::testing::TestWithParam<QuicVersion> {
 protected:
  QuicConnectionTest()
      : connection_id_(42),
        framer_(SupportedVersions(version()), QuicTime::Zero(), false),
        peer_creator_(connection_id_, &framer_, &random_generator_),
        send_algorithm_(new StrictMock<MockSendAlgorithm>),
        loss_algorithm_(new MockLossAlgorithm()),
        helper_(new TestConnectionHelper(&clock_, &random_generator_)),
        writer_(new TestPacketWriter(version())),
        connection_(connection_id_, IPEndPoint(), helper_.get(),
                    writer_.get(), false, version()),
        frame1_(1, false, 0, MakeIOVector(data1)),
        frame2_(1, false, 3, MakeIOVector(data2)),
        sequence_number_length_(PACKET_6BYTE_SEQUENCE_NUMBER),
        connection_id_length_(PACKET_8BYTE_CONNECTION_ID) {
    connection_.set_visitor(&visitor_);
    connection_.SetSendAlgorithm(send_algorithm_);
    connection_.SetLossAlgorithm(loss_algorithm_);
    framer_.set_received_entropy_calculator(&entropy_calculator_);
    // Simplify tests by not sending feedback unless specifically configured.
    SetFeedback(NULL);
    EXPECT_CALL(
        *send_algorithm_, TimeUntilSend(_, _, _)).WillRepeatedly(Return(
            QuicTime::Delta::Zero()));
    EXPECT_CALL(*receive_algorithm_,
                RecordIncomingPacket(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, RetransmissionDelay()).WillRepeatedly(
        Return(QuicTime::Delta::Zero()));
    EXPECT_CALL(*send_algorithm_, GetCongestionWindow()).WillRepeatedly(
        Return(kMaxPacketSize));
    ON_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .WillByDefault(Return(true));
    EXPECT_CALL(visitor_, WillingAndAbleToWrite()).Times(AnyNumber());
    EXPECT_CALL(visitor_, HasPendingHandshake()).Times(AnyNumber());
    EXPECT_CALL(visitor_, OnCanWrite()).Times(AnyNumber());
    EXPECT_CALL(visitor_, HasOpenDataStreams()).WillRepeatedly(Return(false));

    EXPECT_CALL(*loss_algorithm_, GetLossTimeout())
        .WillRepeatedly(Return(QuicTime::Zero()));
    EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
        .WillRepeatedly(Return(SequenceNumberSet()));
  }

  QuicVersion version() {
    return GetParam();
  }

  QuicAckFrame* outgoing_ack() {
    outgoing_ack_.reset(QuicConnectionPeer::CreateAckFrame(&connection_));
    return outgoing_ack_.get();
  }

  QuicPacketSequenceNumber least_unacked() {
    if (version() <= QUIC_VERSION_15) {
      if (writer_->ack_frames().empty()) {
        return 0;
      }
      return writer_->ack_frames()[0].sent_info.least_unacked;
    }
    if (writer_->stop_waiting_frames().empty()) {
      return 0;
    }
    return writer_->stop_waiting_frames()[0].least_unacked;
  }

  void use_tagging_decrypter() {
    writer_->use_tagging_decrypter();
  }

  void ProcessPacket(QuicPacketSequenceNumber number) {
    EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(1);
    ProcessDataPacket(number, 0, !kEntropyFlag);
  }

  QuicPacketEntropyHash ProcessFramePacket(QuicFrame frame) {
    QuicFrames frames;
    frames.push_back(QuicFrame(frame));
    QuicPacketCreatorPeer::SetSendVersionInPacket(&peer_creator_,
                                                  connection_.is_server());
    SerializedPacket serialized_packet =
        peer_creator_.SerializeAllFrames(frames);
    scoped_ptr<QuicPacket> packet(serialized_packet.packet);
    scoped_ptr<QuicEncryptedPacket> encrypted(
        framer_.EncryptPacket(ENCRYPTION_NONE,
                              serialized_packet.sequence_number, *packet));
    connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
    return serialized_packet.entropy_hash;
  }

  size_t ProcessDataPacket(QuicPacketSequenceNumber number,
                           QuicFecGroupNumber fec_group,
                           bool entropy_flag) {
    return ProcessDataPacketAtLevel(number, fec_group, entropy_flag,
                                    ENCRYPTION_NONE);
  }

  size_t ProcessDataPacketAtLevel(QuicPacketSequenceNumber number,
                                  QuicFecGroupNumber fec_group,
                                  bool entropy_flag,
                                  EncryptionLevel level) {
    scoped_ptr<QuicPacket> packet(ConstructDataPacket(number, fec_group,
                                                      entropy_flag));
    scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(
        level, number, *packet));
    connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
    return encrypted->length();
  }

  void ProcessClosePacket(QuicPacketSequenceNumber number,
                          QuicFecGroupNumber fec_group) {
    scoped_ptr<QuicPacket> packet(ConstructClosePacket(number, fec_group));
    scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(
        ENCRYPTION_NONE, number, *packet));
    connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
  }

  size_t ProcessFecProtectedPacket(QuicPacketSequenceNumber number,
                                   bool expect_revival, bool entropy_flag) {
    if (expect_revival) {
      EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(1);
    }
    EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(1).
          RetiresOnSaturation();
    return ProcessDataPacket(number, 1, entropy_flag);
  }

  // Processes an FEC packet that covers the packets that would have been
  // received.
  size_t ProcessFecPacket(QuicPacketSequenceNumber number,
                          QuicPacketSequenceNumber min_protected_packet,
                          bool expect_revival,
                          bool entropy_flag,
                          QuicPacket* packet) {
    if (expect_revival) {
      EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(1);
    }

    // Construct the decrypted data packet so we can compute the correct
    // redundancy. If |packet| has been provided then use that, otherwise
    // construct a default data packet.
    scoped_ptr<QuicPacket> data_packet;
    if (packet) {
      data_packet.reset(packet);
    } else {
      data_packet.reset(ConstructDataPacket(number, 1, !kEntropyFlag));
    }

    header_.public_header.connection_id = connection_id_;
    header_.public_header.reset_flag = false;
    header_.public_header.version_flag = false;
    header_.public_header.sequence_number_length = sequence_number_length_;
    header_.public_header.connection_id_length = connection_id_length_;
    header_.packet_sequence_number = number;
    header_.entropy_flag = entropy_flag;
    header_.fec_flag = true;
    header_.is_in_fec_group = IN_FEC_GROUP;
    header_.fec_group = min_protected_packet;
    QuicFecData fec_data;
    fec_data.fec_group = header_.fec_group;

    // Since all data packets in this test have the same payload, the
    // redundancy is either equal to that payload or the xor of that payload
    // with itself, depending on the number of packets.
    if (((number - min_protected_packet) % 2) == 0) {
      for (size_t i = GetStartOfFecProtectedData(
               header_.public_header.connection_id_length,
               header_.public_header.version_flag,
               header_.public_header.sequence_number_length);
           i < data_packet->length(); ++i) {
        data_packet->mutable_data()[i] ^= data_packet->data()[i];
      }
    }
    fec_data.redundancy = data_packet->FecProtectedData();

    scoped_ptr<QuicPacket> fec_packet(
        framer_.BuildFecPacket(header_, fec_data).packet);
    scoped_ptr<QuicEncryptedPacket> encrypted(
        framer_.EncryptPacket(ENCRYPTION_NONE, number, *fec_packet));

    connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
    return encrypted->length();
  }

  QuicByteCount SendStreamDataToPeer(QuicStreamId id,
                                     StringPiece data,
                                     QuicStreamOffset offset,
                                     bool fin,
                                     QuicPacketSequenceNumber* last_packet) {
    QuicByteCount packet_size;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .WillOnce(DoAll(SaveArg<3>(&packet_size), Return(true)));
    connection_.SendStreamDataWithString(id, data, offset, fin, NULL);
    if (last_packet != NULL) {
      *last_packet =
          QuicConnectionPeer::GetPacketCreator(&connection_)->sequence_number();
    }
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AnyNumber());
    return packet_size;
  }

  void SendAckPacketToPeer() {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendAck();
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AnyNumber());
  }

  QuicPacketEntropyHash ProcessAckPacket(QuicAckFrame* frame) {
    return ProcessFramePacket(QuicFrame(frame));
  }

  QuicPacketEntropyHash ProcessStopWaitingPacket(QuicStopWaitingFrame* frame) {
    return ProcessFramePacket(QuicFrame(frame));
  }

  QuicPacketEntropyHash ProcessGoAwayPacket(QuicGoAwayFrame* frame) {
    return ProcessFramePacket(QuicFrame(frame));
  }

  bool IsMissing(QuicPacketSequenceNumber number) {
    return IsAwaitingPacket(outgoing_ack()->received_info, number);
  }

  QuicPacket* ConstructDataPacket(QuicPacketSequenceNumber number,
                                  QuicFecGroupNumber fec_group,
                                  bool entropy_flag) {
    header_.public_header.connection_id = connection_id_;
    header_.public_header.reset_flag = false;
    header_.public_header.version_flag = false;
    header_.public_header.sequence_number_length = sequence_number_length_;
    header_.public_header.connection_id_length = connection_id_length_;
    header_.entropy_flag = entropy_flag;
    header_.fec_flag = false;
    header_.packet_sequence_number = number;
    header_.is_in_fec_group = fec_group == 0u ? NOT_IN_FEC_GROUP : IN_FEC_GROUP;
    header_.fec_group = fec_group;

    QuicFrames frames;
    QuicFrame frame(&frame1_);
    frames.push_back(frame);
    QuicPacket* packet =
        BuildUnsizedDataPacket(&framer_, header_, frames).packet;
    EXPECT_TRUE(packet != NULL);
    return packet;
  }

  QuicPacket* ConstructClosePacket(QuicPacketSequenceNumber number,
                                   QuicFecGroupNumber fec_group) {
    header_.public_header.connection_id = connection_id_;
    header_.packet_sequence_number = number;
    header_.public_header.reset_flag = false;
    header_.public_header.version_flag = false;
    header_.entropy_flag = false;
    header_.fec_flag = false;
    header_.is_in_fec_group = fec_group == 0u ? NOT_IN_FEC_GROUP : IN_FEC_GROUP;
    header_.fec_group = fec_group;

    QuicConnectionCloseFrame qccf;
    qccf.error_code = QUIC_PEER_GOING_AWAY;

    QuicFrames frames;
    QuicFrame frame(&qccf);
    frames.push_back(frame);
    QuicPacket* packet =
        BuildUnsizedDataPacket(&framer_, header_, frames).packet;
    EXPECT_TRUE(packet != NULL);
    return packet;
  }

  void SetFeedback(QuicCongestionFeedbackFrame* feedback) {
    receive_algorithm_ = new TestReceiveAlgorithm(feedback);
    connection_.SetReceiveAlgorithm(receive_algorithm_);
  }

  QuicTime::Delta DefaultRetransmissionTime() {
    return QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
  }

  QuicTime::Delta DefaultDelayedAckTime() {
    return QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs/2);
  }

  // Initialize a frame acknowledging all packets up to largest_observed.
  const QuicAckFrame InitAckFrame(QuicPacketSequenceNumber largest_observed,
                                  QuicPacketSequenceNumber least_unacked) {
    QuicAckFrame frame(MakeAckFrame(largest_observed, least_unacked));
    if (largest_observed > 0) {
      frame.received_info.entropy_hash =
        QuicConnectionPeer::GetSentEntropyHash(&connection_, largest_observed);
    }
    return frame;
  }

  const QuicStopWaitingFrame InitStopWaitingFrame(
      QuicPacketSequenceNumber least_unacked) {
    QuicStopWaitingFrame frame;
    frame.least_unacked = least_unacked;
    return frame;
  }
  // Explicitly nack a packet.
  void NackPacket(QuicPacketSequenceNumber missing, QuicAckFrame* frame) {
    frame->received_info.missing_packets.insert(missing);
    frame->received_info.entropy_hash ^=
      QuicConnectionPeer::GetSentEntropyHash(&connection_, missing);
    if (missing > 1) {
      frame->received_info.entropy_hash ^=
        QuicConnectionPeer::GetSentEntropyHash(&connection_, missing - 1);
    }
  }

  // Undo nacking a packet within the frame.
  void AckPacket(QuicPacketSequenceNumber arrived, QuicAckFrame* frame) {
    EXPECT_THAT(frame->received_info.missing_packets, Contains(arrived));
    frame->received_info.missing_packets.erase(arrived);
    frame->received_info.entropy_hash ^=
      QuicConnectionPeer::GetSentEntropyHash(&connection_, arrived);
    if (arrived > 1) {
      frame->received_info.entropy_hash ^=
        QuicConnectionPeer::GetSentEntropyHash(&connection_, arrived - 1);
    }
  }

  void TriggerConnectionClose() {
    // Send an erroneous packet to close the connection.
    EXPECT_CALL(visitor_,
                OnConnectionClosed(QUIC_INVALID_PACKET_HEADER, false));
    // Call ProcessDataPacket rather than ProcessPacket, as we should not get a
    // packet call to the visitor.
    ProcessDataPacket(6000, 0, !kEntropyFlag);
    EXPECT_FALSE(
        QuicConnectionPeer::GetConnectionClosePacket(&connection_) == NULL);
  }

  void BlockOnNextWrite() {
    writer_->BlockOnNextWrite();
    EXPECT_CALL(visitor_, OnWriteBlocked()).Times(AtLeast(1));
  }

  void CongestionBlockWrites() {
    EXPECT_CALL(*send_algorithm_,
                TimeUntilSend(_, _, _)).WillRepeatedly(
                    testing::Return(QuicTime::Delta::FromSeconds(1)));
  }

  void CongestionUnblockWrites() {
    EXPECT_CALL(*send_algorithm_,
                TimeUntilSend(_, _, _)).WillRepeatedly(
                    testing::Return(QuicTime::Delta::Zero()));
  }

  QuicConnectionId connection_id_;
  QuicFramer framer_;
  QuicPacketCreator peer_creator_;
  MockEntropyCalculator entropy_calculator_;

  MockSendAlgorithm* send_algorithm_;
  MockLossAlgorithm* loss_algorithm_;
  TestReceiveAlgorithm* receive_algorithm_;
  MockClock clock_;
  MockRandom random_generator_;
  scoped_ptr<TestConnectionHelper> helper_;
  scoped_ptr<TestPacketWriter> writer_;
  TestConnection connection_;
  StrictMock<MockConnectionVisitor> visitor_;

  QuicPacketHeader header_;
  QuicStreamFrame frame1_;
  QuicStreamFrame frame2_;
  scoped_ptr<QuicAckFrame> outgoing_ack_;
  QuicSequenceNumberLength sequence_number_length_;
  QuicConnectionIdLength connection_id_length_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicConnectionTest);
};

// Run all end to end tests with all supported versions.
INSTANTIATE_TEST_CASE_P(SupportedVersion,
                        QuicConnectionTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicConnectionTest, PacketsInOrder) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(1);
  EXPECT_EQ(1u, outgoing_ack()->received_info.largest_observed);
  EXPECT_EQ(0u, outgoing_ack()->received_info.missing_packets.size());

  ProcessPacket(2);
  EXPECT_EQ(2u, outgoing_ack()->received_info.largest_observed);
  EXPECT_EQ(0u, outgoing_ack()->received_info.missing_packets.size());

  ProcessPacket(3);
  EXPECT_EQ(3u, outgoing_ack()->received_info.largest_observed);
  EXPECT_EQ(0u, outgoing_ack()->received_info.missing_packets.size());
}

TEST_P(QuicConnectionTest, PacketsOutOfOrder) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  EXPECT_EQ(3u, outgoing_ack()->received_info.largest_observed);
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(2);
  EXPECT_EQ(3u, outgoing_ack()->received_info.largest_observed);
  EXPECT_FALSE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(1);
  EXPECT_EQ(3u, outgoing_ack()->received_info.largest_observed);
  EXPECT_FALSE(IsMissing(2));
  EXPECT_FALSE(IsMissing(1));
}

TEST_P(QuicConnectionTest, DuplicatePacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  EXPECT_EQ(3u, outgoing_ack()->received_info.largest_observed);
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  // Send packet 3 again, but do not set the expectation that
  // the visitor OnStreamFrames() will be called.
  ProcessDataPacket(3, 0, !kEntropyFlag);
  EXPECT_EQ(3u, outgoing_ack()->received_info.largest_observed);
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));
}

TEST_P(QuicConnectionTest, PacketsOutOfOrderWithAdditionsAndLeastAwaiting) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  EXPECT_EQ(3u, outgoing_ack()->received_info.largest_observed);
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(2);
  EXPECT_EQ(3u, outgoing_ack()->received_info.largest_observed);
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(5);
  EXPECT_EQ(5u, outgoing_ack()->received_info.largest_observed);
  EXPECT_TRUE(IsMissing(1));
  EXPECT_TRUE(IsMissing(4));

  // Pretend at this point the client has gotten acks for 2 and 3 and 1 is a
  // packet the peer will not retransmit.  It indicates this by sending 'least
  // awaiting' is 4.  The connection should then realize 1 will not be
  // retransmitted, and will remove it from the missing list.
  peer_creator_.set_sequence_number(5);
  QuicAckFrame frame = InitAckFrame(1, 4);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _));
  ProcessAckPacket(&frame);

  // Force an ack to be sent.
  SendAckPacketToPeer();
  EXPECT_TRUE(IsMissing(4));
}

TEST_P(QuicConnectionTest, RejectPacketTooFarOut) {
  EXPECT_CALL(visitor_,
              OnConnectionClosed(QUIC_INVALID_PACKET_HEADER, false));
  // Call ProcessDataPacket rather than ProcessPacket, as we should not get a
  // packet call to the visitor.
  ProcessDataPacket(6000, 0, !kEntropyFlag);
  EXPECT_FALSE(
      QuicConnectionPeer::GetConnectionClosePacket(&connection_) == NULL);
}

TEST_P(QuicConnectionTest, RejectUnencryptedStreamData) {
  // Process an unencrypted packet from the non-crypto stream.
  frame1_.stream_id = 3;
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_UNENCRYPTED_STREAM_DATA,
                                           false));
  ProcessDataPacket(1, 0, !kEntropyFlag);
  EXPECT_FALSE(
      QuicConnectionPeer::GetConnectionClosePacket(&connection_) == NULL);
  const vector<QuicConnectionCloseFrame>& connection_close_frames =
      writer_->connection_close_frames();
  EXPECT_EQ(1u, connection_close_frames.size());
  EXPECT_EQ(QUIC_UNENCRYPTED_STREAM_DATA,
            connection_close_frames[0].error_code);
}

TEST_P(QuicConnectionTest, TruncatedAck) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacketSequenceNumber num_packets = 256 * 2 + 1;
  for (QuicPacketSequenceNumber i = 0; i < num_packets; ++i) {
    SendStreamDataToPeer(3, "foo", i * 3, !kFin, NULL);
  }

  QuicAckFrame frame = InitAckFrame(num_packets, 1);
  SequenceNumberSet lost_packets;
  // Create an ack with 256 nacks, none adjacent to one another.
  for (QuicPacketSequenceNumber i = 1; i <= 256; ++i) {
    NackPacket(i * 2, &frame);
    if (i < 256) {  // Last packet is nacked, but not lost.
      lost_packets.insert(i * 2);
    }
  }
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(entropy_calculator_,
              EntropyHash(511)).WillOnce(testing::Return(0));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  ProcessAckPacket(&frame);

  QuicReceivedPacketManager* received_packet_manager =
      QuicConnectionPeer::GetReceivedPacketManager(&connection_);
  // A truncated ack will not have the true largest observed.
  EXPECT_GT(num_packets,
            received_packet_manager->peer_largest_observed_packet());

  AckPacket(192, &frame);

  // Removing one missing packet allows us to ack 192 and one more range, but
  // 192 has already been declared lost, so it doesn't register as an ack.
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(SequenceNumberSet()));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  ProcessAckPacket(&frame);
  EXPECT_EQ(num_packets,
            received_packet_manager->peer_largest_observed_packet());
}

TEST_P(QuicConnectionTest, AckReceiptCausesAckSendBadEntropy) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(1);
  // Delay sending, then queue up an ack.
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillOnce(
                  testing::Return(QuicTime::Delta::FromMicroseconds(1)));
  QuicConnectionPeer::SendAck(&connection_);

  // Process an ack with a least unacked of the received ack.
  // This causes an ack to be sent when TimeUntilSend returns 0.
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillRepeatedly(
                  testing::Return(QuicTime::Delta::Zero()));
  // Skip a packet and then record an ack.
  peer_creator_.set_sequence_number(2);
  QuicAckFrame frame = InitAckFrame(0, 3);
  ProcessAckPacket(&frame);
}

TEST_P(QuicConnectionTest, OutOfOrderReceiptCausesAckSend) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  // Should ack immediately since we have missing packets.
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  ProcessPacket(2);
  // Should ack immediately since we have missing packets.
  EXPECT_EQ(2u, writer_->packets_write_attempts());

  ProcessPacket(1);
  // Should ack immediately, since this fills the last hole.
  EXPECT_EQ(3u, writer_->packets_write_attempts());

  ProcessPacket(4);
  // Should not cause an ack.
  EXPECT_EQ(3u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, AckReceiptCausesAckSend) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicPacketSequenceNumber original;
  QuicByteCount packet_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&original), SaveArg<3>(&packet_size),
                      Return(true)));
  connection_.SendStreamDataWithString(3, "foo", 0, !kFin, NULL);
  QuicAckFrame frame = InitAckFrame(original, 1);
  NackPacket(original, &frame);
  // First nack triggers early retransmit.
  SequenceNumberSet lost_packets;
  lost_packets.insert(1);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicPacketSequenceNumber retransmission;
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, packet_size - kQuicVersionSize, _))
      .WillOnce(DoAll(SaveArg<2>(&retransmission), Return(true)));

  ProcessAckPacket(&frame);

  QuicAckFrame frame2 = InitAckFrame(retransmission, 1);
  NackPacket(original, &frame2);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(SequenceNumberSet()));
  ProcessAckPacket(&frame2);

  // Now if the peer sends an ack which still reports the retransmitted packet
  // as missing, that will bundle an ack with data after two acks in a row
  // indicate the high water mark needs to be raised.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _,
                                             HAS_RETRANSMITTABLE_DATA));
  connection_.SendStreamDataWithString(3, "foo", 3, !kFin, NULL);
  // No ack sent.
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());

  // No more packet loss for the rest of the test.
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillRepeatedly(Return(SequenceNumberSet()));
  ProcessAckPacket(&frame2);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _,
                                             HAS_RETRANSMITTABLE_DATA));
  connection_.SendStreamDataWithString(3, "foo", 3, !kFin, NULL);
  // Ack bundled.
  if (version() > QUIC_VERSION_15) {
    EXPECT_EQ(3u, writer_->frame_count());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
  }
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_FALSE(writer_->ack_frames().empty());

  // But an ack with no missing packets will not send an ack.
  AckPacket(original, &frame2);
  ProcessAckPacket(&frame2);
  ProcessAckPacket(&frame2);
}

TEST_P(QuicConnectionTest, LeastUnackedLower) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  SendStreamDataToPeer(1, "foo", 0, !kFin, NULL);
  SendStreamDataToPeer(1, "bar", 3, !kFin, NULL);
  SendStreamDataToPeer(1, "eep", 6, !kFin, NULL);

  // Start out saying the least unacked is 2.
  peer_creator_.set_sequence_number(5);
  if (version() > QUIC_VERSION_15) {
    QuicStopWaitingFrame frame = InitStopWaitingFrame(2);
    ProcessStopWaitingPacket(&frame);
  } else {
    QuicAckFrame frame = InitAckFrame(0, 2);
    ProcessAckPacket(&frame);
  }

  // Change it to 1, but lower the sequence number to fake out-of-order packets.
  // This should be fine.
  peer_creator_.set_sequence_number(1);
  // The scheduler will not process out of order acks, but all packet processing
  // causes the connection to try to write.
  EXPECT_CALL(visitor_, OnCanWrite());
  if (version() > QUIC_VERSION_15) {
    QuicStopWaitingFrame frame2 = InitStopWaitingFrame(1);
    ProcessStopWaitingPacket(&frame2);
  } else {
    QuicAckFrame frame2 = InitAckFrame(0, 1);
    ProcessAckPacket(&frame2);
  }

  // Now claim it's one, but set the ordering so it was sent "after" the first
  // one.  This should cause a connection error.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  peer_creator_.set_sequence_number(7);
  if (version() > QUIC_VERSION_15) {
    EXPECT_CALL(visitor_,
                OnConnectionClosed(QUIC_INVALID_STOP_WAITING_DATA, false));
    QuicStopWaitingFrame frame2 = InitStopWaitingFrame(1);
    ProcessStopWaitingPacket(&frame2);
  } else {
    EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_INVALID_ACK_DATA, false));
    QuicAckFrame frame2 = InitAckFrame(0, 1);
    ProcessAckPacket(&frame2);
  }
}

TEST_P(QuicConnectionTest, LargestObservedLower) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  SendStreamDataToPeer(1, "foo", 0, !kFin, NULL);
  SendStreamDataToPeer(1, "bar", 3, !kFin, NULL);
  SendStreamDataToPeer(1, "eep", 6, !kFin, NULL);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));

  // Start out saying the largest observed is 2.
  QuicAckFrame frame1 = InitAckFrame(1, 0);
  QuicAckFrame frame2 = InitAckFrame(2, 0);
  ProcessAckPacket(&frame2);

  // Now change it to 1, and it should cause a connection error.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_INVALID_ACK_DATA, false));
  EXPECT_CALL(visitor_, OnCanWrite()).Times(0);
  ProcessAckPacket(&frame1);
}

TEST_P(QuicConnectionTest, AckUnsentData) {
  // Ack a packet which has not been sent.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_INVALID_ACK_DATA, false));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  QuicAckFrame frame(MakeAckFrame(1, 0));
  EXPECT_CALL(visitor_, OnCanWrite()).Times(0);
  ProcessAckPacket(&frame);
}

TEST_P(QuicConnectionTest, AckAll) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);

  peer_creator_.set_sequence_number(1);
  QuicAckFrame frame1 = InitAckFrame(0, 1);
  ProcessAckPacket(&frame1);
}

TEST_P(QuicConnectionTest, SendingDifferentSequenceNumberLengthsBandwidth) {
  QuicPacketSequenceNumber last_packet;
  QuicPacketCreator* creator =
      QuicConnectionPeer::GetPacketCreator(&connection_);
  SendStreamDataToPeer(1, "foo", 0, !kFin, &last_packet);
  EXPECT_EQ(1u, last_packet);
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            creator->next_sequence_number_length());
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            writer_->header().public_header.sequence_number_length);

  EXPECT_CALL(*send_algorithm_, GetCongestionWindow()).WillRepeatedly(
      Return(kMaxPacketSize * 256));

  SendStreamDataToPeer(1, "bar", 3, !kFin, &last_packet);
  EXPECT_EQ(2u, last_packet);
  EXPECT_EQ(PACKET_2BYTE_SEQUENCE_NUMBER,
            creator->next_sequence_number_length());
  // The 1 packet lag is due to the sequence number length being recalculated in
  // QuicConnection after a packet is sent.
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            writer_->header().public_header.sequence_number_length);

  EXPECT_CALL(*send_algorithm_, GetCongestionWindow()).WillRepeatedly(
      Return(kMaxPacketSize * 256 * 256));

  SendStreamDataToPeer(1, "foo", 6, !kFin, &last_packet);
  EXPECT_EQ(3u, last_packet);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            creator->next_sequence_number_length());
  EXPECT_EQ(PACKET_2BYTE_SEQUENCE_NUMBER,
            writer_->header().public_header.sequence_number_length);

  EXPECT_CALL(*send_algorithm_, GetCongestionWindow()).WillRepeatedly(
      Return(kMaxPacketSize * 256 * 256 * 256));

  SendStreamDataToPeer(1, "bar", 9, !kFin, &last_packet);
  EXPECT_EQ(4u, last_packet);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            creator->next_sequence_number_length());
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            writer_->header().public_header.sequence_number_length);

  EXPECT_CALL(*send_algorithm_, GetCongestionWindow()).WillRepeatedly(
      Return(kMaxPacketSize * 256 * 256 * 256 * 256));

  SendStreamDataToPeer(1, "foo", 12, !kFin, &last_packet);
  EXPECT_EQ(5u, last_packet);
  EXPECT_EQ(PACKET_6BYTE_SEQUENCE_NUMBER,
            creator->next_sequence_number_length());
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            writer_->header().public_header.sequence_number_length);
}

TEST_P(QuicConnectionTest, SendingDifferentSequenceNumberLengthsUnackedDelta) {
  QuicPacketSequenceNumber last_packet;
  QuicPacketCreator* creator =
      QuicConnectionPeer::GetPacketCreator(&connection_);
  SendStreamDataToPeer(1, "foo", 0, !kFin, &last_packet);
  EXPECT_EQ(1u, last_packet);
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            creator->next_sequence_number_length());
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            writer_->header().public_header.sequence_number_length);

  creator->set_sequence_number(100);

  SendStreamDataToPeer(1, "bar", 3, !kFin, &last_packet);
  EXPECT_EQ(PACKET_2BYTE_SEQUENCE_NUMBER,
            creator->next_sequence_number_length());
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            writer_->header().public_header.sequence_number_length);

  creator->set_sequence_number(100 * 256);

  SendStreamDataToPeer(1, "foo", 6, !kFin, &last_packet);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            creator->next_sequence_number_length());
  EXPECT_EQ(PACKET_2BYTE_SEQUENCE_NUMBER,
            writer_->header().public_header.sequence_number_length);

  creator->set_sequence_number(100 * 256 * 256);

  SendStreamDataToPeer(1, "bar", 9, !kFin, &last_packet);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            creator->next_sequence_number_length());
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            writer_->header().public_header.sequence_number_length);

  creator->set_sequence_number(100 * 256 * 256 * 256);

  SendStreamDataToPeer(1, "foo", 12, !kFin, &last_packet);
  EXPECT_EQ(PACKET_6BYTE_SEQUENCE_NUMBER,
            creator->next_sequence_number_length());
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            writer_->header().public_header.sequence_number_length);
}

TEST_P(QuicConnectionTest, BasicSending) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacketSequenceNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, !kFin, &last_packet);  // Packet 1
  EXPECT_EQ(1u, last_packet);
  SendAckPacketToPeer();  // Packet 2

  EXPECT_EQ(1u, least_unacked());

  SendAckPacketToPeer();  // Packet 3
  EXPECT_EQ(1u, least_unacked());

  SendStreamDataToPeer(1, "bar", 3, !kFin, &last_packet);  // Packet 4
  EXPECT_EQ(4u, last_packet);
  SendAckPacketToPeer();  // Packet 5
  EXPECT_EQ(1u, least_unacked());

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));

  // Peer acks up to packet 3.
  QuicAckFrame frame = InitAckFrame(3, 0);
  ProcessAckPacket(&frame);
  SendAckPacketToPeer();  // Packet 6

  // As soon as we've acked one, we skip ack packets 2 and 3 and note lack of
  // ack for 4.
  EXPECT_EQ(4u, least_unacked());

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));

  // Peer acks up to packet 4, the last packet.
  QuicAckFrame frame2 = InitAckFrame(6, 0);
  ProcessAckPacket(&frame2);  // Acks don't instigate acks.

  // Verify that we did not send an ack.
  EXPECT_EQ(6u, writer_->header().packet_sequence_number);

  // So the last ack has not changed.
  EXPECT_EQ(4u, least_unacked());

  // If we force an ack, we shouldn't change our retransmit state.
  SendAckPacketToPeer();  // Packet 7
  EXPECT_EQ(7u, least_unacked());

  // But if we send more data it should.
  SendStreamDataToPeer(1, "eep", 6, !kFin, &last_packet);  // Packet 8
  EXPECT_EQ(8u, last_packet);
  SendAckPacketToPeer();  // Packet 9
  EXPECT_EQ(7u, least_unacked());
}

TEST_P(QuicConnectionTest, FECSending) {
  // All packets carry version info till version is negotiated.
  QuicPacketCreator* creator =
      QuicConnectionPeer::GetPacketCreator(&connection_);
  size_t payload_length;
  // GetPacketLengthForOneStream() assumes a stream offset of 0 in determining
  // packet length. The size of the offset field in a stream frame is 0 for
  // offset 0, and 2 for non-zero offsets up through 64K. Increase
  // max_packet_length by 2 so that subsequent packets containing subsequent
  // stream frames with non-zero offets will fit within the packet length.
  size_t length = 2 + GetPacketLengthForOneStream(
          connection_.version(), kIncludeVersion, PACKET_1BYTE_SEQUENCE_NUMBER,
          IN_FEC_GROUP, &payload_length);
  creator->set_max_packet_length(length);

  // Enable FEC.
  creator->set_max_packets_per_fec_group(2);

  // Send 4 protected data packets, which will also trigger 2 FEC packets.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(6);
  // The first stream frame will have 2 fewer overhead bytes than the other 3.
  const string payload(payload_length * 4 + 2, 'a');
  connection_.SendStreamDataWithStringWithFec(1, payload, 0, !kFin, NULL);
  // Expect the FEC group to be closed after SendStreamDataWithString.
  EXPECT_FALSE(creator->IsFecGroupOpen());
  EXPECT_FALSE(creator->IsFecProtected());
}

TEST_P(QuicConnectionTest, FECQueueing) {
  // All packets carry version info till version is negotiated.
  size_t payload_length;
  QuicPacketCreator* creator =
      QuicConnectionPeer::GetPacketCreator(&connection_);
  size_t length = GetPacketLengthForOneStream(
      connection_.version(), kIncludeVersion, PACKET_1BYTE_SEQUENCE_NUMBER,
      IN_FEC_GROUP, &payload_length);
  creator->set_max_packet_length(length);
  // Enable FEC.
  creator->set_max_packets_per_fec_group(1);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  BlockOnNextWrite();
  const string payload(payload_length, 'a');
  connection_.SendStreamDataWithStringWithFec(1, payload, 0, !kFin, NULL);
  EXPECT_FALSE(creator->IsFecGroupOpen());
  EXPECT_FALSE(creator->IsFecProtected());
  // Expect the first data packet and the fec packet to be queued.
  EXPECT_EQ(2u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, AbandonFECFromCongestionWindow) {
  // Enable FEC.
  QuicConnectionPeer::GetPacketCreator(
      &connection_)->set_max_packets_per_fec_group(1);

  // 1 Data and 1 FEC packet.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  connection_.SendStreamDataWithStringWithFec(3, "foo", 0, !kFin, NULL);

  const QuicTime::Delta retransmission_time =
      QuicTime::Delta::FromMilliseconds(5000);
  clock_.AdvanceTime(retransmission_time);

  // Abandon FEC packet and data packet.
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_CALL(visitor_, OnCanWrite());
  connection_.OnRetransmissionTimeout();
}

TEST_P(QuicConnectionTest, DontAbandonAckedFEC) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Enable FEC.
  QuicConnectionPeer::GetPacketCreator(
      &connection_)->set_max_packets_per_fec_group(1);

  // 1 Data and 1 FEC packet.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(6);
  connection_.SendStreamDataWithStringWithFec(3, "foo", 0, !kFin, NULL);
  // Send some more data afterwards to ensure early retransmit doesn't trigger.
  connection_.SendStreamDataWithStringWithFec(3, "foo", 3, !kFin, NULL);
  connection_.SendStreamDataWithStringWithFec(3, "foo", 6, !kFin, NULL);

  QuicAckFrame ack_fec = InitAckFrame(2, 1);
  // Data packet missing.
  // TODO(ianswett): Note that this is not a sensible ack, since if the FEC was
  // received, it would cause the covered packet to be acked as well.
  NackPacket(1, &ack_fec);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  ProcessAckPacket(&ack_fec);
  clock_.AdvanceTime(DefaultRetransmissionTime());

  // Don't abandon the acked FEC packet, but it will abandon 2 the subsequent
  // FEC packets.
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(3);
  connection_.GetRetransmissionAlarm()->Fire();
}

TEST_P(QuicConnectionTest, AbandonAllFEC) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Enable FEC.
  QuicConnectionPeer::GetPacketCreator(
      &connection_)->set_max_packets_per_fec_group(1);

  // 1 Data and 1 FEC packet.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(6);
  connection_.SendStreamDataWithStringWithFec(3, "foo", 0, !kFin, NULL);
  // Send some more data afterwards to ensure early retransmit doesn't trigger.
  connection_.SendStreamDataWithStringWithFec(3, "foo", 3, !kFin, NULL);
  // Advance the time so not all the FEC packets are abandoned.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
  connection_.SendStreamDataWithStringWithFec(3, "foo", 6, !kFin, NULL);

  QuicAckFrame ack_fec = InitAckFrame(5, 1);
  // Ack all data packets, but no fec packets.
  NackPacket(2, &ack_fec);
  NackPacket(4, &ack_fec);

  // Lose the first FEC packet and ack the three data packets.
  SequenceNumberSet lost_packets;
  lost_packets.insert(2);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  ProcessAckPacket(&ack_fec);

  clock_.AdvanceTime(DefaultRetransmissionTime().Subtract(
      QuicTime::Delta::FromMilliseconds(1)));

  // Abandon all packets
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(false));
  connection_.GetRetransmissionAlarm()->Fire();

  // Ensure the alarm is not set since all packets have been abandoned.
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, FramePacking) {
  CongestionBlockWrites();

  // Send an ack and two stream frames in 1 packet by queueing them.
  connection_.SendAck();
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(DoAll(
      IgnoreResult(InvokeWithoutArgs(&connection_,
                                     &TestConnection::SendStreamData3)),
      IgnoreResult(InvokeWithoutArgs(&connection_,
                                     &TestConnection::SendStreamData5))));

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  CongestionUnblockWrites();
  connection_.GetSendAlarm()->Fire();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's an ack and two stream frames from
  // two different streams.
  if (version() > QUIC_VERSION_15) {
    EXPECT_EQ(4u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(3u, writer_->frame_count());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  ASSERT_EQ(2u, writer_->stream_frames().size());
  EXPECT_EQ(kClientDataStreamId1, writer_->stream_frames()[0].stream_id);
  EXPECT_EQ(kClientDataStreamId2, writer_->stream_frames()[1].stream_id);
}

TEST_P(QuicConnectionTest, FramePackingNonCryptoThenCrypto) {
  CongestionBlockWrites();

  // Send an ack and two stream frames (one non-crypto, then one crypto) in 2
  // packets by queueing them.
  connection_.SendAck();
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(DoAll(
      IgnoreResult(InvokeWithoutArgs(&connection_,
                                     &TestConnection::SendStreamData3)),
      IgnoreResult(InvokeWithoutArgs(&connection_,
                                     &TestConnection::SendCryptoStreamData))));

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  CongestionUnblockWrites();
  connection_.GetSendAlarm()->Fire();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's the crypto stream frame.
  EXPECT_EQ(1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(kCryptoStreamId, writer_->stream_frames()[0].stream_id);
}

TEST_P(QuicConnectionTest, FramePackingCryptoThenNonCrypto) {
  CongestionBlockWrites();

  // Send an ack and two stream frames (one crypto, then one non-crypto) in 2
  // packets by queueing them.
  connection_.SendAck();
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(DoAll(
      IgnoreResult(InvokeWithoutArgs(&connection_,
                                     &TestConnection::SendCryptoStreamData)),
      IgnoreResult(InvokeWithoutArgs(&connection_,
                                     &TestConnection::SendStreamData3))));

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  CongestionUnblockWrites();
  connection_.GetSendAlarm()->Fire();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's the stream frame from stream 3.
  EXPECT_EQ(1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(kClientDataStreamId1, writer_->stream_frames()[0].stream_id);
}

TEST_P(QuicConnectionTest, FramePackingFEC) {
  // Enable FEC.
  QuicConnectionPeer::GetPacketCreator(
      &connection_)->set_max_packets_per_fec_group(6);

  CongestionBlockWrites();

  // Queue an ack and two stream frames. Ack gets flushed when FEC is turned on
  // for sending protected data; two stream frames are packing in 1 packet.
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(DoAll(
      IgnoreResult(InvokeWithoutArgs(
          &connection_, &TestConnection::SendStreamData3WithFec)),
      IgnoreResult(InvokeWithoutArgs(
          &connection_, &TestConnection::SendStreamData5WithFec))));
  connection_.SendAck();

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(3);
  CongestionUnblockWrites();
  connection_.GetSendAlarm()->Fire();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's in an fec group.
  EXPECT_EQ(2u, writer_->header().fec_group);
  EXPECT_EQ(0u, writer_->frame_count());
}

TEST_P(QuicConnectionTest, FramePackingAckResponse) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Process a data packet to queue up a pending ack.
  EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(1);
  ProcessDataPacket(1, 1, kEntropyFlag);

  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(DoAll(
      IgnoreResult(InvokeWithoutArgs(&connection_,
                                     &TestConnection::SendStreamData3)),
      IgnoreResult(InvokeWithoutArgs(&connection_,
                                     &TestConnection::SendStreamData5))));

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);

  // Process an ack to cause the visitor's OnCanWrite to be invoked.
  peer_creator_.set_sequence_number(2);
  QuicAckFrame ack_one = InitAckFrame(0, 0);
  ProcessAckPacket(&ack_one);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's an ack and two stream frames from
  // two different streams.
  if (version() > QUIC_VERSION_15) {
    EXPECT_EQ(4u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(3u, writer_->frame_count());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  ASSERT_EQ(2u, writer_->stream_frames().size());
  EXPECT_EQ(kClientDataStreamId1, writer_->stream_frames()[0].stream_id);
  EXPECT_EQ(kClientDataStreamId2, writer_->stream_frames()[1].stream_id);
}

TEST_P(QuicConnectionTest, FramePackingSendv) {
  // Send data in 1 packet by writing multiple blocks in a single iovector
  // using writev.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));

  char data[] = "ABCD";
  IOVector data_iov;
  data_iov.AppendNoCoalesce(data, 2);
  data_iov.AppendNoCoalesce(data + 2, 2);
  connection_.SendStreamData(1, data_iov, 0, !kFin, MAY_FEC_PROTECT, NULL);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure multiple iovector blocks have
  // been packed into a single stream frame from one stream.
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  QuicStreamFrame frame = writer_->stream_frames()[0];
  EXPECT_EQ(1u, frame.stream_id);
  EXPECT_EQ("ABCD", string(static_cast<char*>
                           (frame.data.iovec()[0].iov_base),
                           (frame.data.iovec()[0].iov_len)));
}

TEST_P(QuicConnectionTest, FramePackingSendvQueued) {
  // Try to send two stream frames in 1 packet by using writev.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));

  BlockOnNextWrite();
  char data[] = "ABCD";
  IOVector data_iov;
  data_iov.AppendNoCoalesce(data, 2);
  data_iov.AppendNoCoalesce(data + 2, 2);
  connection_.SendStreamData(1, data_iov, 0, !kFin, MAY_FEC_PROTECT, NULL);

  EXPECT_EQ(1u, connection_.NumQueuedPackets());
  EXPECT_TRUE(connection_.HasQueuedData());

  // Unblock the writes and actually send.
  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());

  // Parse the last packet and ensure it's one stream frame from one stream.
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(1u, writer_->stream_frames()[0].stream_id);
}

TEST_P(QuicConnectionTest, SendingZeroBytes) {
  // Send a zero byte write with a fin using writev.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  IOVector empty_iov;
  connection_.SendStreamData(1, empty_iov, 0, kFin, MAY_FEC_PROTECT, NULL);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's one stream frame from one stream.
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(1u, writer_->stream_frames()[0].stream_id);
  EXPECT_TRUE(writer_->stream_frames()[0].fin);
}

TEST_P(QuicConnectionTest, OnCanWrite) {
  // Visitor's OnCanWrite will send data, but will have more pending writes.
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(DoAll(
      IgnoreResult(InvokeWithoutArgs(&connection_,
                                     &TestConnection::SendStreamData3)),
      IgnoreResult(InvokeWithoutArgs(&connection_,
                                     &TestConnection::SendStreamData5))));
  EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillOnce(Return(true));
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillRepeatedly(
                  testing::Return(QuicTime::Delta::Zero()));

  connection_.OnCanWrite();

  // Parse the last packet and ensure it's the two stream frames from
  // two different streams.
  EXPECT_EQ(2u, writer_->frame_count());
  EXPECT_EQ(2u, writer_->stream_frames().size());
  EXPECT_EQ(kClientDataStreamId1, writer_->stream_frames()[0].stream_id);
  EXPECT_EQ(kClientDataStreamId2, writer_->stream_frames()[1].stream_id);
}

TEST_P(QuicConnectionTest, RetransmitOnNack) {
  QuicPacketSequenceNumber last_packet;
  QuicByteCount second_packet_size;
  SendStreamDataToPeer(3, "foo", 0, !kFin, &last_packet);  // Packet 1
  second_packet_size =
      SendStreamDataToPeer(3, "foos", 3, !kFin, &last_packet);  // Packet 2
  SendStreamDataToPeer(3, "fooos", 7, !kFin, &last_packet);  // Packet 3

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Don't lose a packet on an ack, and nothing is retransmitted.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicAckFrame ack_one = InitAckFrame(1, 0);
  ProcessAckPacket(&ack_one);

  // Lose a packet and ensure it triggers retransmission.
  QuicAckFrame nack_two = InitAckFrame(3, 0);
  NackPacket(2, &nack_two);
  SequenceNumberSet lost_packets;
  lost_packets.insert(2);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, second_packet_size - kQuicVersionSize, _)).
                  Times(1);
  ProcessAckPacket(&nack_two);
}

TEST_P(QuicConnectionTest, DiscardRetransmit) {
  QuicPacketSequenceNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, !kFin, &last_packet);  // Packet 1
  SendStreamDataToPeer(1, "foos", 3, !kFin, &last_packet);  // Packet 2
  SendStreamDataToPeer(1, "fooos", 7, !kFin, &last_packet);  // Packet 3

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Instigate a loss with an ack.
  QuicAckFrame nack_two = InitAckFrame(3, 0);
  NackPacket(2, &nack_two);
  // The first nack should trigger a fast retransmission, but we'll be
  // write blocked, so the packet will be queued.
  BlockOnNextWrite();
  SequenceNumberSet lost_packets;
  lost_packets.insert(2);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  ProcessAckPacket(&nack_two);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Now, ack the previous transmission.
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(SequenceNumberSet()));
  QuicAckFrame ack_all = InitAckFrame(3, 0);
  ProcessAckPacket(&ack_all);

  // Unblock the socket and attempt to send the queued packets.  However,
  // since the previous transmission has been acked, we will not
  // send the retransmission.
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, _)).Times(0);

  writer_->SetWritable();
  connection_.OnCanWrite();

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, RetransmitNackedLargestObserved) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacketSequenceNumber largest_observed;
  QuicByteCount packet_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&largest_observed), SaveArg<3>(&packet_size),
                      Return(true)));
  connection_.SendStreamDataWithString(3, "foo", 0, !kFin, NULL);

  QuicAckFrame frame = InitAckFrame(1, largest_observed);
  NackPacket(largest_observed, &frame);
  // The first nack should retransmit the largest observed packet.
  SequenceNumberSet lost_packets;
  lost_packets.insert(1);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, packet_size - kQuicVersionSize, _));
  ProcessAckPacket(&frame);
}

TEST_P(QuicConnectionTest, QueueAfterTwoRTOs) {
  for (int i = 0; i < 10; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendStreamDataWithString(3, "foo", i * 3, !kFin, NULL);
  }

  // Block the congestion window and ensure they're queued.
  BlockOnNextWrite();
  clock_.AdvanceTime(DefaultRetransmissionTime());
  // Only one packet should be retransmitted.
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_TRUE(connection_.HasQueuedData());

  // Unblock the congestion window.
  writer_->SetWritable();
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(
      2 * DefaultRetransmissionTime().ToMicroseconds()));
  // Retransmit already retransmitted packets event though the sequence number
  // greater than the largest observed.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(10);
  connection_.GetRetransmissionAlarm()->Fire();
  connection_.OnCanWrite();
}

TEST_P(QuicConnectionTest, WriteBlockedThenSent) {
  BlockOnNextWrite();
  writer_->set_is_write_blocked_data_buffered(true);
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, NULL);
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.OnPacketSent(WriteResult(WRITE_STATUS_OK, 0));
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, WriteBlockedAckedThenSent) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  BlockOnNextWrite();
  writer_->set_is_write_blocked_data_buffered(true);
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, NULL);
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Ack the sent packet before the callback returns, which happens in
  // rare circumstances with write blocked sockets.
  QuicAckFrame ack = InitAckFrame(1, 0);
  ProcessAckPacket(&ack);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.OnPacketSent(WriteResult(WRITE_STATUS_OK, 0));
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, RetransmitWriteBlockedAckedOriginalThenSent) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  connection_.SendStreamDataWithString(3, "foo", 0, !kFin, NULL);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  BlockOnNextWrite();
  writer_->set_is_write_blocked_data_buffered(true);
  // Simulate the retransmission alarm firing.
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(_));
  clock_.AdvanceTime(DefaultRetransmissionTime());
  connection_.GetRetransmissionAlarm()->Fire();

  // Ack the sent packet before the callback returns, which happens in
  // rare circumstances with write blocked sockets.
  QuicAckFrame ack = InitAckFrame(1, 0);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  ProcessAckPacket(&ack);

  connection_.OnPacketSent(WriteResult(WRITE_STATUS_OK, 0));
  // There is now a pending packet, but with no retransmittable frames.
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(connection_.sent_packet_manager().HasRetransmittableFrames(2));
}

TEST_P(QuicConnectionTest, AlarmsWhenWriteBlocked) {
  // Block the connection.
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(3, "foo", 0, !kFin, NULL);
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_TRUE(writer_->IsWriteBlocked());

  // Set the send and resumption alarms. Fire the alarms and ensure they don't
  // attempt to write.
  connection_.GetResumeWritesAlarm()->Set(clock_.ApproximateNow());
  connection_.GetSendAlarm()->Set(clock_.ApproximateNow());
  connection_.GetResumeWritesAlarm()->Fire();
  connection_.GetSendAlarm()->Fire();
  EXPECT_TRUE(writer_->IsWriteBlocked());
  EXPECT_EQ(1u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, NoLimitPacketsPerNack) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  int offset = 0;
  // Send packets 1 to 15.
  for (int i = 0; i < 15; ++i) {
    SendStreamDataToPeer(1, "foo", offset, !kFin, NULL);
    offset += 3;
  }

  // Ack 15, nack 1-14.
  SequenceNumberSet lost_packets;
  QuicAckFrame nack = InitAckFrame(15, 0);
  for (int i = 1; i < 15; ++i) {
    NackPacket(i, &nack);
    lost_packets.insert(i);
  }

  // 14 packets have been NACK'd and lost.  In TCP cubic, PRR limits
  // the retransmission rate in the case of burst losses.
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(14);
  ProcessAckPacket(&nack);
}

// Test sending multiple acks from the connection to the session.
TEST_P(QuicConnectionTest, MultipleAcks) {
  QuicPacketSequenceNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, !kFin, &last_packet);  // Packet 1
  EXPECT_EQ(1u, last_packet);
  SendStreamDataToPeer(3, "foo", 0, !kFin, &last_packet);  // Packet 2
  EXPECT_EQ(2u, last_packet);
  SendAckPacketToPeer();  // Packet 3
  SendStreamDataToPeer(5, "foo", 0, !kFin, &last_packet);  // Packet 4
  EXPECT_EQ(4u, last_packet);
  SendStreamDataToPeer(1, "foo", 3, !kFin, &last_packet);  // Packet 5
  EXPECT_EQ(5u, last_packet);
  SendStreamDataToPeer(3, "foo", 3, !kFin, &last_packet);  // Packet 6
  EXPECT_EQ(6u, last_packet);

  // Client will ack packets 1, 2, [!3], 4, 5.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicAckFrame frame1 = InitAckFrame(5, 0);
  NackPacket(3, &frame1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessAckPacket(&frame1);

  // Now the client implicitly acks 3, and explicitly acks 6.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicAckFrame frame2 = InitAckFrame(6, 0);
  ProcessAckPacket(&frame2);
}

TEST_P(QuicConnectionTest, DontLatchUnackedPacket) {
  SendStreamDataToPeer(1, "foo", 0, !kFin, NULL);  // Packet 1;
  // From now on, we send acks, so the send algorithm won't mark them pending.
  ON_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
              .WillByDefault(Return(false));
  SendAckPacketToPeer();  // Packet 2

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicAckFrame frame = InitAckFrame(1, 0);
  ProcessAckPacket(&frame);

  // Verify that our internal state has least-unacked as 2, because we're still
  // waiting for a potential ack for 2.
  EXPECT_EQ(2u, outgoing_ack()->sent_info.least_unacked);

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  frame = InitAckFrame(2, 0);
  ProcessAckPacket(&frame);
  EXPECT_EQ(3u, outgoing_ack()->sent_info.least_unacked);

  // When we send an ack, we make sure our least-unacked makes sense.  In this
  // case since we're not waiting on an ack for 2 and all packets are acked, we
  // set it to 3.
  SendAckPacketToPeer();  // Packet 3
  // Least_unacked remains at 3 until another ack is received.
  EXPECT_EQ(3u, outgoing_ack()->sent_info.least_unacked);
  // Check that the outgoing ack had its sequence number as least_unacked.
  EXPECT_EQ(3u, least_unacked());

  // Ack the ack, which updates the rtt and raises the least unacked.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  frame = InitAckFrame(3, 0);
  ProcessAckPacket(&frame);

  ON_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
              .WillByDefault(Return(true));
  SendStreamDataToPeer(1, "bar", 3, false, NULL);  // Packet 4
  EXPECT_EQ(4u, outgoing_ack()->sent_info.least_unacked);
  ON_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
              .WillByDefault(Return(false));
  SendAckPacketToPeer();  // Packet 5
  EXPECT_EQ(4u, least_unacked());

  // Send two data packets at the end, and ensure if the last one is acked,
  // the least unacked is raised above the ack packets.
  ON_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
              .WillByDefault(Return(true));
  SendStreamDataToPeer(1, "bar", 6, false, NULL);  // Packet 6
  SendStreamDataToPeer(1, "bar", 9, false, NULL);  // Packet 7

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  frame = InitAckFrame(7, 0);
  NackPacket(5, &frame);
  NackPacket(6, &frame);
  ProcessAckPacket(&frame);

  EXPECT_EQ(6u, outgoing_ack()->sent_info.least_unacked);
}

TEST_P(QuicConnectionTest, ReviveMissingPacketAfterFecPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Don't send missing packet 1.
  ProcessFecPacket(2, 1, true, !kEntropyFlag, NULL);
  // Entropy flag should be false, so entropy should be 0.
  EXPECT_EQ(0u, QuicConnectionPeer::ReceivedEntropyHash(&connection_, 2));
}

TEST_P(QuicConnectionTest, ReviveMissingPacketWithVaryingSeqNumLengths) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Set up a debug visitor to the connection.
  scoped_ptr<FecQuicConnectionDebugVisitor>
      fec_visitor(new FecQuicConnectionDebugVisitor);
  connection_.set_debug_visitor(fec_visitor.get());

  QuicPacketSequenceNumber fec_packet = 0;
  QuicSequenceNumberLength lengths[] = {PACKET_6BYTE_SEQUENCE_NUMBER,
                                        PACKET_4BYTE_SEQUENCE_NUMBER,
                                        PACKET_2BYTE_SEQUENCE_NUMBER,
                                        PACKET_1BYTE_SEQUENCE_NUMBER};
  // For each sequence number length size, revive a packet and check sequence
  // number length in the revived packet.
  for (size_t i = 0; i < arraysize(lengths); ++i) {
    // Set sequence_number_length_ (for data and FEC packets).
    sequence_number_length_ = lengths[i];
    fec_packet += 2;
    // Don't send missing packet, but send fec packet right after it.
    ProcessFecPacket(fec_packet, fec_packet - 1, true, !kEntropyFlag, NULL);
    // Sequence number length in the revived header should be the same as
    // in the original data/fec packet headers.
    EXPECT_EQ(sequence_number_length_, fec_visitor->revived_header().
                                       public_header.sequence_number_length);
  }
}

TEST_P(QuicConnectionTest, ReviveMissingPacketWithVaryingConnectionIdLengths) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Set up a debug visitor to the connection.
  scoped_ptr<FecQuicConnectionDebugVisitor>
      fec_visitor(new FecQuicConnectionDebugVisitor);
  connection_.set_debug_visitor(fec_visitor.get());

  QuicPacketSequenceNumber fec_packet = 0;
  QuicConnectionIdLength lengths[] = {PACKET_8BYTE_CONNECTION_ID,
                                      PACKET_4BYTE_CONNECTION_ID,
                                      PACKET_1BYTE_CONNECTION_ID,
                                      PACKET_0BYTE_CONNECTION_ID};
  // For each connection id length size, revive a packet and check connection
  // id length in the revived packet.
  for (size_t i = 0; i < arraysize(lengths); ++i) {
    // Set connection id length (for data and FEC packets).
    connection_id_length_ = lengths[i];
    fec_packet += 2;
    // Don't send missing packet, but send fec packet right after it.
    ProcessFecPacket(fec_packet, fec_packet - 1, true, !kEntropyFlag, NULL);
    // Connection id length in the revived header should be the same as
    // in the original data/fec packet headers.
    EXPECT_EQ(connection_id_length_,
              fec_visitor->revived_header().public_header.connection_id_length);
  }
}

TEST_P(QuicConnectionTest, ReviveMissingPacketAfterDataPacketThenFecPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessFecProtectedPacket(1, false, kEntropyFlag);
  // Don't send missing packet 2.
  ProcessFecPacket(3, 1, true, !kEntropyFlag, NULL);
  // Entropy flag should be true, so entropy should not be 0.
  EXPECT_NE(0u, QuicConnectionPeer::ReceivedEntropyHash(&connection_, 2));
}

TEST_P(QuicConnectionTest, ReviveMissingPacketAfterDataPacketsThenFecPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessFecProtectedPacket(1, false, !kEntropyFlag);
  // Don't send missing packet 2.
  ProcessFecProtectedPacket(3, false, !kEntropyFlag);
  ProcessFecPacket(4, 1, true, kEntropyFlag, NULL);
  // Ensure QUIC no longer revives entropy for lost packets.
  EXPECT_EQ(0u, QuicConnectionPeer::ReceivedEntropyHash(&connection_, 2));
  EXPECT_NE(0u, QuicConnectionPeer::ReceivedEntropyHash(&connection_, 4));
}

TEST_P(QuicConnectionTest, ReviveMissingPacketAfterDataPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Don't send missing packet 1.
  ProcessFecPacket(3, 1, false, !kEntropyFlag, NULL);
  // Out of order.
  ProcessFecProtectedPacket(2, true, !kEntropyFlag);
  // Entropy flag should be false, so entropy should be 0.
  EXPECT_EQ(0u, QuicConnectionPeer::ReceivedEntropyHash(&connection_, 2));
}

TEST_P(QuicConnectionTest, ReviveMissingPacketAfterDataPackets) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessFecProtectedPacket(1, false, !kEntropyFlag);
  // Don't send missing packet 2.
  ProcessFecPacket(6, 1, false, kEntropyFlag, NULL);
  ProcessFecProtectedPacket(3, false, kEntropyFlag);
  ProcessFecProtectedPacket(4, false, kEntropyFlag);
  ProcessFecProtectedPacket(5, true, !kEntropyFlag);
  // Ensure entropy is not revived for the missing packet.
  EXPECT_EQ(0u, QuicConnectionPeer::ReceivedEntropyHash(&connection_, 2));
  EXPECT_NE(0u, QuicConnectionPeer::ReceivedEntropyHash(&connection_, 3));
}

TEST_P(QuicConnectionTest, TLP) {
  QuicSentPacketManagerPeer::SetMaxTailLossProbes(
      QuicConnectionPeer::GetSentPacketManager(&connection_), 1);

  SendStreamDataToPeer(3, "foo", 0, !kFin, NULL);
  EXPECT_EQ(1u, outgoing_ack()->sent_info.least_unacked);
  QuicTime retransmission_time =
      connection_.GetRetransmissionAlarm()->deadline();
  EXPECT_NE(QuicTime::Zero(), retransmission_time);

  EXPECT_EQ(1u, writer_->header().packet_sequence_number);
  // Simulate the retransmission alarm firing and sending a tlp,
  // so send algorithm's OnRetransmissionTimeout is not called.
  clock_.AdvanceTime(retransmission_time.Subtract(clock_.Now()));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 2u, _, _));
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(2u, writer_->header().packet_sequence_number);
  // We do not raise the high water mark yet.
  EXPECT_EQ(1u, outgoing_ack()->sent_info.least_unacked);
}

TEST_P(QuicConnectionTest, RTO) {
  QuicTime default_retransmission_time = clock_.ApproximateNow().Add(
      DefaultRetransmissionTime());
  SendStreamDataToPeer(3, "foo", 0, !kFin, NULL);
  EXPECT_EQ(1u, outgoing_ack()->sent_info.least_unacked);

  EXPECT_EQ(1u, writer_->header().packet_sequence_number);
  EXPECT_EQ(default_retransmission_time,
            connection_.GetRetransmissionAlarm()->deadline());
  // Simulate the retransmission alarm firing.
  clock_.AdvanceTime(DefaultRetransmissionTime());
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 2u, _, _));
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(2u, writer_->header().packet_sequence_number);
  // We do not raise the high water mark yet.
  EXPECT_EQ(1u, outgoing_ack()->sent_info.least_unacked);
}

TEST_P(QuicConnectionTest, RTOWithSameEncryptionLevel) {
  QuicTime default_retransmission_time = clock_.ApproximateNow().Add(
      DefaultRetransmissionTime());
  use_tagging_decrypter();

  // A TaggingEncrypter puts kTagSize copies of the given byte (0x01 here) at
  // the end of the packet. We can test this to check which encrypter was used.
  connection_.SetEncrypter(ENCRYPTION_NONE, new TaggingEncrypter(0x01));
  SendStreamDataToPeer(3, "foo", 0, !kFin, NULL);
  EXPECT_EQ(0x01010101u, writer_->final_bytes_of_last_packet());

  connection_.SetEncrypter(ENCRYPTION_INITIAL, new TaggingEncrypter(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  SendStreamDataToPeer(3, "foo", 0, !kFin, NULL);
  EXPECT_EQ(0x02020202u, writer_->final_bytes_of_last_packet());

  EXPECT_EQ(default_retransmission_time,
            connection_.GetRetransmissionAlarm()->deadline());
  {
    InSequence s;
    EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 3, _, _));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 4, _, _));
  }

  // Simulate the retransmission alarm firing.
  clock_.AdvanceTime(DefaultRetransmissionTime());
  connection_.GetRetransmissionAlarm()->Fire();

  // Packet should have been sent with ENCRYPTION_NONE.
  EXPECT_EQ(0x01010101u, writer_->final_bytes_of_previous_packet());

  // Packet should have been sent with ENCRYPTION_INITIAL.
  EXPECT_EQ(0x02020202u, writer_->final_bytes_of_last_packet());
}

TEST_P(QuicConnectionTest, SendHandshakeMessages) {
  use_tagging_decrypter();
  // A TaggingEncrypter puts kTagSize copies of the given byte (0x01 here) at
  // the end of the packet. We can test this to check which encrypter was used.
  connection_.SetEncrypter(ENCRYPTION_NONE, new TaggingEncrypter(0x01));

  // Attempt to send a handshake message and have the socket block.
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillRepeatedly(
                  testing::Return(QuicTime::Delta::Zero()));
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, NULL);
  // The packet should be serialized, but not queued.
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Switch to the new encrypter.
  connection_.SetEncrypter(ENCRYPTION_INITIAL, new TaggingEncrypter(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);

  // Now become writeable and flush the packets.
  writer_->SetWritable();
  EXPECT_CALL(visitor_, OnCanWrite());
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());

  // Verify that the handshake packet went out at the null encryption.
  EXPECT_EQ(0x01010101u, writer_->final_bytes_of_last_packet());
}

TEST_P(QuicConnectionTest,
       DropRetransmitsForNullEncryptedPacketAfterForwardSecure) {
  use_tagging_decrypter();
  connection_.SetEncrypter(ENCRYPTION_NONE, new TaggingEncrypter(0x01));
  QuicPacketSequenceNumber sequence_number;
  SendStreamDataToPeer(3, "foo", 0, !kFin, &sequence_number);

  // Simulate the retransmission alarm firing and the socket blocking.
  BlockOnNextWrite();
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  clock_.AdvanceTime(DefaultRetransmissionTime());
  connection_.GetRetransmissionAlarm()->Fire();

  // Go forward secure.
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           new TaggingEncrypter(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.NeuterUnencryptedPackets();

  EXPECT_EQ(QuicTime::Zero(),
            connection_.GetRetransmissionAlarm()->deadline());
  // Unblock the socket and ensure that no packets are sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  writer_->SetWritable();
  connection_.OnCanWrite();
}

TEST_P(QuicConnectionTest, RetransmitPacketsWithInitialEncryption) {
  use_tagging_decrypter();
  connection_.SetEncrypter(ENCRYPTION_NONE, new TaggingEncrypter(0x01));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_NONE);

  SendStreamDataToPeer(1, "foo", 0, !kFin, NULL);

  connection_.SetEncrypter(ENCRYPTION_INITIAL, new TaggingEncrypter(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);

  SendStreamDataToPeer(2, "bar", 0, !kFin, NULL);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);

  connection_.RetransmitUnackedPackets(INITIAL_ENCRYPTION_ONLY);
}

TEST_P(QuicConnectionTest, BufferNonDecryptablePackets) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  use_tagging_decrypter();

  const uint8 tag = 0x07;
  framer_.SetEncrypter(ENCRYPTION_INITIAL, new TaggingEncrypter(tag));

  // Process an encrypted packet which can not yet be decrypted
  // which should result in the packet being buffered.
  ProcessDataPacketAtLevel(1, 0, kEntropyFlag, ENCRYPTION_INITIAL);

  // Transition to the new encryption state and process another
  // encrypted packet which should result in the original packet being
  // processed.
  connection_.SetDecrypter(new StrictTaggingDecrypter(tag),
                           ENCRYPTION_INITIAL);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.SetEncrypter(ENCRYPTION_INITIAL, new TaggingEncrypter(tag));
  EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(2);
  ProcessDataPacketAtLevel(2, 0, kEntropyFlag, ENCRYPTION_INITIAL);

  // Finally, process a third packet and note that we do not
  // reprocess the buffered packet.
  EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(1);
  ProcessDataPacketAtLevel(3, 0, kEntropyFlag, ENCRYPTION_INITIAL);
}

TEST_P(QuicConnectionTest, TestRetransmitOrder) {
  QuicByteCount first_packet_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).WillOnce(
      DoAll(SaveArg<3>(&first_packet_size), Return(true)));

  connection_.SendStreamDataWithString(3, "first_packet", 0, !kFin, NULL);
  QuicByteCount second_packet_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).WillOnce(
      DoAll(SaveArg<3>(&second_packet_size), Return(true)));
  connection_.SendStreamDataWithString(3, "second_packet", 12, !kFin, NULL);
  EXPECT_NE(first_packet_size, second_packet_size);
  // Advance the clock by huge time to make sure packets will be retransmitted.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(10));
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  {
    InSequence s;
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, _, _, first_packet_size, _));
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, _, _, second_packet_size, _));
  }
  connection_.GetRetransmissionAlarm()->Fire();

  // Advance again and expect the packets to be sent again in the same order.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(20));
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  {
    InSequence s;
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, _, _, first_packet_size, _));
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, _, _, second_packet_size, _));
  }
  connection_.GetRetransmissionAlarm()->Fire();
}

TEST_P(QuicConnectionTest, RetransmissionCountCalculation) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacketSequenceNumber original_sequence_number;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&original_sequence_number), Return(true)));
  connection_.SendStreamDataWithString(3, "foo", 0, !kFin, NULL);

  EXPECT_TRUE(QuicConnectionPeer::IsSavedForRetransmission(
      &connection_, original_sequence_number));
  EXPECT_FALSE(QuicConnectionPeer::IsRetransmission(
      &connection_, original_sequence_number));
  // Force retransmission due to RTO.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(10));
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  QuicPacketSequenceNumber rto_sequence_number;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&rto_sequence_number), Return(true)));
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_FALSE(QuicConnectionPeer::IsSavedForRetransmission(
      &connection_, original_sequence_number));
  ASSERT_TRUE(QuicConnectionPeer::IsSavedForRetransmission(
      &connection_, rto_sequence_number));
  EXPECT_TRUE(QuicConnectionPeer::IsRetransmission(
      &connection_, rto_sequence_number));
  // Once by explicit nack.
  SequenceNumberSet lost_packets;
  lost_packets.insert(rto_sequence_number);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicPacketSequenceNumber nack_sequence_number = 0;
  // Ack packets might generate some other packets, which are not
  // retransmissions. (More ack packets).
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&nack_sequence_number), Return(true)));
  QuicAckFrame ack = InitAckFrame(rto_sequence_number, 0);
  // Nack the retransmitted packet.
  NackPacket(original_sequence_number, &ack);
  NackPacket(rto_sequence_number, &ack);
  ProcessAckPacket(&ack);

  ASSERT_NE(0u, nack_sequence_number);
  EXPECT_FALSE(QuicConnectionPeer::IsSavedForRetransmission(
      &connection_, rto_sequence_number));
  ASSERT_TRUE(QuicConnectionPeer::IsSavedForRetransmission(
      &connection_, nack_sequence_number));
  EXPECT_TRUE(QuicConnectionPeer::IsRetransmission(
      &connection_, nack_sequence_number));
}

TEST_P(QuicConnectionTest, SetRTOAfterWritingToSocket) {
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, NULL);
  // Make sure that RTO is not started when the packet is queued.
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Test that RTO is started once we write to the socket.
  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, DelayRTOWithAckReceipt) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(2);
  connection_.SendStreamDataWithString(2, "foo", 0, !kFin, NULL);
  connection_.SendStreamDataWithString(3, "bar", 0, !kFin, NULL);
  QuicAlarm* retransmission_alarm = connection_.GetRetransmissionAlarm();
  EXPECT_TRUE(retransmission_alarm->IsSet());
  EXPECT_EQ(clock_.Now().Add(DefaultRetransmissionTime()),
            retransmission_alarm->deadline());

  // Advance the time right before the RTO, then receive an ack for the first
  // packet to delay the RTO.
  clock_.AdvanceTime(DefaultRetransmissionTime());
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicAckFrame ack = InitAckFrame(1, 0);
  ProcessAckPacket(&ack);
  EXPECT_TRUE(retransmission_alarm->IsSet());
  EXPECT_GT(retransmission_alarm->deadline(), clock_.Now());

  // Move forward past the original RTO and ensure the RTO is still pending.
  clock_.AdvanceTime(DefaultRetransmissionTime().Multiply(2));

  // Ensure the second packet gets retransmitted when it finally fires.
  EXPECT_TRUE(retransmission_alarm->IsSet());
  EXPECT_LT(retransmission_alarm->deadline(), clock_.ApproximateNow());
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  // Manually cancel the alarm to simulate a real test.
  connection_.GetRetransmissionAlarm()->Fire();

  // The new retransmitted sequence number should set the RTO to a larger value
  // than previously.
  EXPECT_TRUE(retransmission_alarm->IsSet());
  QuicTime next_rto_time = retransmission_alarm->deadline();
  QuicTime expected_rto_time =
      connection_.sent_packet_manager().GetRetransmissionTime();
  EXPECT_EQ(next_rto_time, expected_rto_time);
}

TEST_P(QuicConnectionTest, TestQueued) {
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, NULL);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Unblock the writes and actually send.
  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, CloseFecGroup) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Don't send missing packet 1.
  // Don't send missing packet 2.
  ProcessFecProtectedPacket(3, false, !kEntropyFlag);
  // Don't send missing FEC packet 3.
  ASSERT_EQ(1u, connection_.NumFecGroups());

  // Now send non-fec protected ack packet and close the group.
  peer_creator_.set_sequence_number(4);
  if (version() > QUIC_VERSION_15) {
    QuicStopWaitingFrame frame = InitStopWaitingFrame(5);
    ProcessStopWaitingPacket(&frame);
  } else {
    QuicAckFrame frame = InitAckFrame(0, 5);
    ProcessAckPacket(&frame);
  }
  ASSERT_EQ(0u, connection_.NumFecGroups());
}

TEST_P(QuicConnectionTest, NoQuicCongestionFeedbackFrame) {
  SendAckPacketToPeer();
  EXPECT_TRUE(writer_->feedback_frames().empty());
}

TEST_P(QuicConnectionTest, WithQuicCongestionFeedbackFrame) {
  QuicCongestionFeedbackFrame info;
  info.type = kFixRate;
  info.fix_rate.bitrate = QuicBandwidth::FromBytesPerSecond(123);
  SetFeedback(&info);

  SendAckPacketToPeer();
  ASSERT_FALSE(writer_->feedback_frames().empty());
  ASSERT_EQ(kFixRate, writer_->feedback_frames()[0].type);
  ASSERT_EQ(info.fix_rate.bitrate,
            writer_->feedback_frames()[0].fix_rate.bitrate);
}

TEST_P(QuicConnectionTest, UpdateQuicCongestionFeedbackFrame) {
  SendAckPacketToPeer();
  EXPECT_CALL(*receive_algorithm_, RecordIncomingPacket(_, _, _));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
}

TEST_P(QuicConnectionTest, DontUpdateQuicCongestionFeedbackFrameForRevived) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  SendAckPacketToPeer();
  // Process an FEC packet, and revive the missing data packet
  // but only contact the receive_algorithm once.
  EXPECT_CALL(*receive_algorithm_, RecordIncomingPacket(_, _, _));
  ProcessFecPacket(2, 1, true, !kEntropyFlag, NULL);
}

TEST_P(QuicConnectionTest, InitialTimeout) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_CONNECTION_TIMED_OUT, false));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));

  QuicTime default_timeout = clock_.ApproximateNow().Add(
      QuicTime::Delta::FromSeconds(kDefaultInitialTimeoutSecs));
  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  // Simulate the timeout alarm firing.
  clock_.AdvanceTime(
      QuicTime::Delta::FromSeconds(kDefaultInitialTimeoutSecs));
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());

  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetResumeWritesAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, PingAfterSend) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, HasOpenDataStreams()).WillRepeatedly(Return(true));
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());

  // Advance to 5ms, and send a packet to the peer, which will set
  // the ping alarm.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  SendStreamDataToPeer(1, "GET /", 0, kFin, NULL);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(clock_.ApproximateNow().Add(QuicTime::Delta::FromSeconds(15)),
            connection_.GetPingAlarm()->deadline());

  // Now recevie and ACK of the previous packet, which will move the
  // ping alarm forward.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  QuicAckFrame frame = InitAckFrame(1, 0);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(clock_.ApproximateNow().Add(QuicTime::Delta::FromSeconds(15)),
            connection_.GetPingAlarm()->deadline());

  writer_->Reset();
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(15));
  connection_.GetPingAlarm()->Fire();
  EXPECT_EQ(1u, writer_->frame_count());
  if (version() > QUIC_VERSION_17) {
    ASSERT_EQ(1u, writer_->ping_frames().size());
  } else {
    ASSERT_EQ(1u, writer_->stream_frames().size());
    EXPECT_EQ(kCryptoStreamId, writer_->stream_frames()[0].stream_id);
    EXPECT_EQ(0u, writer_->stream_frames()[0].offset);
  }
  writer_->Reset();

  EXPECT_CALL(visitor_, HasOpenDataStreams()).WillRepeatedly(Return(false));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  SendAckPacketToPeer();

  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, TimeoutAfterSend) {
  EXPECT_TRUE(connection_.connected());

  QuicTime default_timeout = clock_.ApproximateNow().Add(
      QuicTime::Delta::FromSeconds(kDefaultInitialTimeoutSecs));

  // When we send a packet, the timeout will change to 5000 +
  // kDefaultInitialTimeoutSecs.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));

  // Send an ack so we don't set the retransmission alarm.
  SendAckPacketToPeer();
  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5000.  The alarm will reregister.
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(
      kDefaultInitialTimeoutSecs * 1000000 - 5000));
  EXPECT_EQ(default_timeout, clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(default_timeout.Add(QuicTime::Delta::FromMilliseconds(5)),
            connection_.GetTimeoutAlarm()->deadline());

  // This time, we should time out.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_CONNECTION_TIMED_OUT, false));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(default_timeout.Add(QuicTime::Delta::FromMilliseconds(5)),
            clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, SendScheduler) {
  // Test that if we send a packet without delay, it is not queued.
  QuicPacket* packet = ConstructDataPacket(1, 0, !kEntropyFlag);
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillOnce(
                  testing::Return(QuicTime::Delta::Zero()));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendPacket(
      ENCRYPTION_NONE, 1, packet, kTestEntropyHash, HAS_RETRANSMITTABLE_DATA);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, SendSchedulerDelay) {
  // Test that if we send a packet with a delay, it ends up queued.
  QuicPacket* packet = ConstructDataPacket(1, 0, !kEntropyFlag);
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillOnce(
                  testing::Return(QuicTime::Delta::FromMicroseconds(1)));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _)).Times(0);
  connection_.SendPacket(
      ENCRYPTION_NONE, 1, packet, kTestEntropyHash, HAS_RETRANSMITTABLE_DATA);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, SendSchedulerEAGAIN) {
  QuicPacket* packet = ConstructDataPacket(1, 0, !kEntropyFlag);
  BlockOnNextWrite();
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillOnce(
                  testing::Return(QuicTime::Delta::Zero()));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _)).Times(0);
  connection_.SendPacket(
      ENCRYPTION_NONE, 1, packet, kTestEntropyHash, HAS_RETRANSMITTABLE_DATA);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, SendSchedulerDelayThenSend) {
  // Test that if we send a packet with a delay, it ends up queued.
  QuicPacket* packet = ConstructDataPacket(1, 0, !kEntropyFlag);
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillOnce(
                  testing::Return(QuicTime::Delta::FromMicroseconds(1)));
  connection_.SendPacket(
       ENCRYPTION_NONE, 1, packet, kTestEntropyHash, HAS_RETRANSMITTABLE_DATA);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Advance the clock to fire the alarm, and configure the scheduler
  // to permit the packet to be sent.
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillRepeatedly(
                  testing::Return(QuicTime::Delta::Zero()));
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(1));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.GetSendAlarm()->Fire();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, SendSchedulerDelayThenRetransmit) {
  CongestionUnblockWrites();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _));
  connection_.SendStreamDataWithString(3, "foo", 0, !kFin, NULL);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  // Advance the time for retransmission of lost packet.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(501));
  // Test that if we send a retransmit with a delay, it ends up queued in the
  // sent packet manager, but not yet serialized.
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  CongestionBlockWrites();
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());

  // Advance the clock to fire the alarm, and configure the scheduler
  // to permit the packet to be sent.
  CongestionUnblockWrites();

  // Ensure the scheduler is notified this is a retransmit.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(1));
  connection_.GetSendAlarm()->Fire();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, SendSchedulerDelayAndQueue) {
  QuicPacket* packet = ConstructDataPacket(1, 0, !kEntropyFlag);
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillOnce(
                  testing::Return(QuicTime::Delta::FromMicroseconds(1)));
  connection_.SendPacket(
      ENCRYPTION_NONE, 1, packet, kTestEntropyHash, HAS_RETRANSMITTABLE_DATA);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Attempt to send another packet and make sure that it gets queued.
  packet = ConstructDataPacket(2, 0, !kEntropyFlag);
  connection_.SendPacket(
      ENCRYPTION_NONE, 2, packet, kTestEntropyHash, HAS_RETRANSMITTABLE_DATA);
  EXPECT_EQ(2u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, SendSchedulerDelayThenAckAndSend) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacket* packet = ConstructDataPacket(1, 0, !kEntropyFlag);
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillOnce(
                  testing::Return(QuicTime::Delta::FromMicroseconds(10)));
  connection_.SendPacket(
      ENCRYPTION_NONE, 1, packet, kTestEntropyHash, HAS_RETRANSMITTABLE_DATA);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Now send non-retransmitting information, that we're not going to
  // retransmit 3. The far end should stop waiting for it.
  QuicAckFrame frame = InitAckFrame(0, 1);
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillRepeatedly(
                  testing::Return(QuicTime::Delta::Zero()));
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, _));
  ProcessAckPacket(&frame);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  // Ensure alarm is not set
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendSchedulerDelayThenAckAndHold) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacket* packet = ConstructDataPacket(1, 0, !kEntropyFlag);
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillOnce(
                  testing::Return(QuicTime::Delta::FromMicroseconds(10)));
  connection_.SendPacket(
      ENCRYPTION_NONE, 1, packet, kTestEntropyHash, HAS_RETRANSMITTABLE_DATA);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Now send non-retransmitting information, that we're not going to
  // retransmit 3.  The far end should stop waiting for it.
  QuicAckFrame frame = InitAckFrame(0, 1);
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillOnce(
                  testing::Return(QuicTime::Delta::FromMicroseconds(1)));
  ProcessAckPacket(&frame);

  EXPECT_EQ(1u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, SendSchedulerDelayThenOnCanWrite) {
  // TODO(ianswett): This test is unrealistic, because we would not serialize
  // new data if the send algorithm said not to.
  QuicPacket* packet = ConstructDataPacket(1, 0, !kEntropyFlag);
  CongestionBlockWrites();
  connection_.SendPacket(
      ENCRYPTION_NONE, 1, packet, kTestEntropyHash, HAS_RETRANSMITTABLE_DATA);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // OnCanWrite should send the packet, because it won't consult the send
  // algorithm for queued packets.
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, TestQueueLimitsOnSendStreamData) {
  // All packets carry version info till version is negotiated.
  size_t payload_length;
  size_t length = GetPacketLengthForOneStream(
      connection_.version(), kIncludeVersion, PACKET_1BYTE_SEQUENCE_NUMBER,
      NOT_IN_FEC_GROUP, &payload_length);
  QuicConnectionPeer::GetPacketCreator(&connection_)->set_max_packet_length(
      length);

  // Queue the first packet.
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, _, _)).WillOnce(
                  testing::Return(QuicTime::Delta::FromMicroseconds(10)));
  const string payload(payload_length, 'a');
  EXPECT_EQ(0u,
            connection_.SendStreamDataWithString(3, payload, 0,
                                                 !kFin, NULL).bytes_consumed);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, LoopThroughSendingPackets) {
  // All packets carry version info till version is negotiated.
  size_t payload_length;
  // GetPacketLengthForOneStream() assumes a stream offset of 0 in determining
  // packet length. The size of the offset field in a stream frame is 0 for
  // offset 0, and 2 for non-zero offsets up through 16K. Increase
  // max_packet_length by 2 so that subsequent packets containing subsequent
  // stream frames with non-zero offets will fit within the packet length.
  size_t length = 2 + GetPacketLengthForOneStream(
          connection_.version(), kIncludeVersion, PACKET_1BYTE_SEQUENCE_NUMBER,
          NOT_IN_FEC_GROUP, &payload_length);
  QuicConnectionPeer::GetPacketCreator(&connection_)->set_max_packet_length(
      length);

  // Queue the first packet.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(7);
  // The first stream frame will have 2 fewer overhead bytes than the other six.
  const string payload(payload_length * 7 + 2, 'a');
  EXPECT_EQ(payload.size(),
            connection_.SendStreamDataWithString(1, payload, 0,
                                                 !kFin, NULL).bytes_consumed);
}

TEST_P(QuicConnectionTest, SendDelayedAck) {
  QuicTime ack_time = clock_.ApproximateNow().Add(DefaultDelayedAckTime());
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  const uint8 tag = 0x07;
  connection_.SetDecrypter(new StrictTaggingDecrypter(tag),
                           ENCRYPTION_INITIAL);
  framer_.SetEncrypter(ENCRYPTION_INITIAL, new TaggingEncrypter(tag));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // The same as ProcessPacket(1) except that ENCRYPTION_INITIAL is used
  // instead of ENCRYPTION_NONE.
  EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(1);
  ProcessDataPacketAtLevel(1, 0, !kEntropyFlag, ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
  // Simulate delayed ack alarm firing.
  connection_.GetAckAlarm()->Fire();
  // Check that ack is sent and that delayed ack alarm is reset.
  if (version() > QUIC_VERSION_15) {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(1u, writer_->frame_count());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendEarlyDelayedAckForCrypto) {
  QuicTime ack_time = clock_.ApproximateNow();
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  // Process a packet from the crypto stream, which is frame1_'s default.
  ProcessPacket(1);
  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
  // Simulate delayed ack alarm firing.
  connection_.GetAckAlarm()->Fire();
  // Check that ack is sent and that delayed ack alarm is reset.
  if (version() > QUIC_VERSION_15) {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(1u, writer_->frame_count());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendDelayedAckOnSecondPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  ProcessPacket(2);
  // Check that ack is sent and that delayed ack alarm is reset.
  if (version() > QUIC_VERSION_15) {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(1u, writer_->frame_count());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, NoAckOnOldNacks) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Drop one packet, triggering a sequence of acks.
  ProcessPacket(2);
  size_t frames_per_ack = version() > QUIC_VERSION_15 ? 2 : 1;
  EXPECT_EQ(frames_per_ack, writer_->frame_count());
  EXPECT_FALSE(writer_->ack_frames().empty());
  writer_->Reset();
  ProcessPacket(3);
  EXPECT_EQ(frames_per_ack, writer_->frame_count());
  EXPECT_FALSE(writer_->ack_frames().empty());
  writer_->Reset();
  ProcessPacket(4);
  EXPECT_EQ(frames_per_ack, writer_->frame_count());
  EXPECT_FALSE(writer_->ack_frames().empty());
  writer_->Reset();
  ProcessPacket(5);
  EXPECT_EQ(frames_per_ack, writer_->frame_count());
  EXPECT_FALSE(writer_->ack_frames().empty());
  writer_->Reset();
  // Now only set the timer on the 6th packet, instead of sending another ack.
  ProcessPacket(6);
  EXPECT_EQ(0u, writer_->frame_count());
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendDelayedAckOnOutgoingPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  connection_.SendStreamDataWithString(kClientDataStreamId1, "foo", 0,
                                       !kFin, NULL);
  // Check that ack is bundled with outgoing data and that delayed ack
  // alarm is reset.
  if (version() > QUIC_VERSION_15) {
    EXPECT_EQ(3u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendDelayedAckOnOutgoingCryptoPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  connection_.SendStreamDataWithString(kCryptoStreamId, "foo", 0, !kFin, NULL);
  // Check that ack is bundled with outgoing crypto data.
  EXPECT_EQ(version() <= QUIC_VERSION_15 ? 2u : 3u, writer_->frame_count());
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, BundleAckForSecondCHLO) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(
      IgnoreResult(InvokeWithoutArgs(&connection_,
                                     &TestConnection::SendCryptoStreamData)));
  // Process a packet from the crypto stream, which is frame1_'s default.
  // Receiving the CHLO as packet 2 first will cause the connection to
  // immediately send an ack, due to the packet gap.
  ProcessPacket(2);
  // Check that ack is sent and that delayed ack alarm is reset.
  if (version() > QUIC_VERSION_15) {
    EXPECT_EQ(3u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
  }
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, BundleAckWithDataOnIncomingAck) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  connection_.SendStreamDataWithString(kClientDataStreamId1, "foo", 0,
                                       !kFin, NULL);
  connection_.SendStreamDataWithString(kClientDataStreamId1, "foo", 3,
                                       !kFin, NULL);
  // Ack the second packet, which will retransmit the first packet.
  QuicAckFrame ack = InitAckFrame(2, 0);
  NackPacket(1, &ack);
  SequenceNumberSet lost_packets;
  lost_packets.insert(1);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  ProcessAckPacket(&ack);
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  writer_->Reset();

  // Now ack the retransmission, which will both raise the high water mark
  // and see if there is more data to send.
  ack = InitAckFrame(3, 0);
  NackPacket(1, &ack);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(SequenceNumberSet()));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  ProcessAckPacket(&ack);

  // Check that no packet is sent and the ack alarm isn't set.
  EXPECT_EQ(0u, writer_->frame_count());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  writer_->Reset();

  // Send the same ack, but send both data and an ack together.
  ack = InitAckFrame(3, 0);
  NackPacket(1, &ack);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(SequenceNumberSet()));
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(
      IgnoreResult(InvokeWithoutArgs(
          &connection_,
          &TestConnection::EnsureWritableAndSendStreamData5)));
  ProcessAckPacket(&ack);

  // Check that ack is bundled with outgoing data and the delayed ack
  // alarm is reset.
  if (version() > QUIC_VERSION_15) {
    EXPECT_EQ(3u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, NoAckSentForClose) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PEER_GOING_AWAY, true));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessClosePacket(2, 0);
}

TEST_P(QuicConnectionTest, SendWhenDisconnected) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PEER_GOING_AWAY, false));
  connection_.CloseConnection(QUIC_PEER_GOING_AWAY, false);
  EXPECT_FALSE(connection_.connected());
  QuicPacket* packet = ConstructDataPacket(1, 0, !kEntropyFlag);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _)).Times(0);
  connection_.SendPacket(
      ENCRYPTION_NONE, 1, packet, kTestEntropyHash, HAS_RETRANSMITTABLE_DATA);
}

TEST_P(QuicConnectionTest, PublicReset) {
  QuicPublicResetPacket header;
  header.public_header.connection_id = connection_id_;
  header.public_header.reset_flag = true;
  header.public_header.version_flag = false;
  header.rejected_sequence_number = 10101;
  scoped_ptr<QuicEncryptedPacket> packet(
      framer_.BuildPublicResetPacket(header));
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PUBLIC_RESET, true));
  connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *packet);
}

TEST_P(QuicConnectionTest, GoAway) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicGoAwayFrame goaway;
  goaway.last_good_stream_id = 1;
  goaway.error_code = QUIC_PEER_GOING_AWAY;
  goaway.reason_phrase = "Going away.";
  EXPECT_CALL(visitor_, OnGoAway(_));
  ProcessGoAwayPacket(&goaway);
}

TEST_P(QuicConnectionTest, WindowUpdate) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicWindowUpdateFrame window_update;
  window_update.stream_id = 3;
  window_update.byte_offset = 1234;
  EXPECT_CALL(visitor_, OnWindowUpdateFrames(_));
  ProcessFramePacket(QuicFrame(&window_update));
}

TEST_P(QuicConnectionTest, Blocked) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicBlockedFrame blocked;
  blocked.stream_id = 3;
  EXPECT_CALL(visitor_, OnBlockedFrames(_));
  ProcessFramePacket(QuicFrame(&blocked));
}

TEST_P(QuicConnectionTest, InvalidPacket) {
  EXPECT_CALL(visitor_,
              OnConnectionClosed(QUIC_INVALID_PACKET_HEADER, false));
  QuicEncryptedPacket encrypted(NULL, 0);
  connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), encrypted);
  // The connection close packet should have error details.
  ASSERT_FALSE(writer_->connection_close_frames().empty());
  EXPECT_EQ("Unable to read public flags.",
            writer_->connection_close_frames()[0].error_details);
}

TEST_P(QuicConnectionTest, MissingPacketsBeforeLeastUnacked) {
  // Set the sequence number of the ack packet to be least unacked (4).
  peer_creator_.set_sequence_number(3);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  if (version() > QUIC_VERSION_15) {
    QuicStopWaitingFrame frame = InitStopWaitingFrame(4);
    ProcessStopWaitingPacket(&frame);
  } else {
    QuicAckFrame ack = InitAckFrame(0, 4);
    ProcessAckPacket(&ack);
  }
  EXPECT_TRUE(outgoing_ack()->received_info.missing_packets.empty());
}

TEST_P(QuicConnectionTest, ReceivedEntropyHashCalculation) {
  EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(AtLeast(1));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessDataPacket(1, 1, kEntropyFlag);
  ProcessDataPacket(4, 1, kEntropyFlag);
  ProcessDataPacket(3, 1, !kEntropyFlag);
  ProcessDataPacket(7, 1, kEntropyFlag);
  EXPECT_EQ(146u, outgoing_ack()->received_info.entropy_hash);
}

TEST_P(QuicConnectionTest, ReceivedEntropyHashCalculationHalfFEC) {
  // FEC packets should not change the entropy hash calculation.
  EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(AtLeast(1));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessDataPacket(1, 1, kEntropyFlag);
  ProcessFecPacket(4, 1, false, kEntropyFlag, NULL);
  ProcessDataPacket(3, 3, !kEntropyFlag);
  ProcessFecPacket(7, 3, false, kEntropyFlag, NULL);
  EXPECT_EQ(146u, outgoing_ack()->received_info.entropy_hash);
}

TEST_P(QuicConnectionTest, UpdateEntropyForReceivedPackets) {
  EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(AtLeast(1));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessDataPacket(1, 1, kEntropyFlag);
  ProcessDataPacket(5, 1, kEntropyFlag);
  ProcessDataPacket(4, 1, !kEntropyFlag);
  EXPECT_EQ(34u, outgoing_ack()->received_info.entropy_hash);
  // Make 4th packet my least unacked, and update entropy for 2, 3 packets.
  peer_creator_.set_sequence_number(5);
  QuicPacketEntropyHash six_packet_entropy_hash = 0;
  QuicPacketEntropyHash kRandomEntropyHash = 129u;
  if (version() > QUIC_VERSION_15) {
    QuicStopWaitingFrame frame = InitStopWaitingFrame(4);
    frame.entropy_hash = kRandomEntropyHash;
    if (ProcessStopWaitingPacket(&frame)) {
      six_packet_entropy_hash = 1 << 6;
    }
  } else {
    QuicAckFrame ack = InitAckFrame(0, 4);
    ack.sent_info.entropy_hash = kRandomEntropyHash;
    if (ProcessAckPacket(&ack)) {
      six_packet_entropy_hash = 1 << 6;
    }
  }

  EXPECT_EQ((kRandomEntropyHash + (1 << 5) + six_packet_entropy_hash),
            outgoing_ack()->received_info.entropy_hash);
}

TEST_P(QuicConnectionTest, UpdateEntropyHashUptoCurrentPacket) {
  EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(AtLeast(1));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessDataPacket(1, 1, kEntropyFlag);
  ProcessDataPacket(5, 1, !kEntropyFlag);
  ProcessDataPacket(22, 1, kEntropyFlag);
  EXPECT_EQ(66u, outgoing_ack()->received_info.entropy_hash);
  peer_creator_.set_sequence_number(22);
  QuicPacketEntropyHash kRandomEntropyHash = 85u;
  // Current packet is the least unacked packet.
  QuicPacketEntropyHash ack_entropy_hash;
  if (version() > QUIC_VERSION_15) {
    QuicStopWaitingFrame frame = InitStopWaitingFrame(23);
    frame.entropy_hash = kRandomEntropyHash;
    ack_entropy_hash = ProcessStopWaitingPacket(&frame);
  } else {
    QuicAckFrame ack = InitAckFrame(0, 23);
    ack.sent_info.entropy_hash = kRandomEntropyHash;
    ack_entropy_hash = ProcessAckPacket(&ack);
  }
  EXPECT_EQ((kRandomEntropyHash + ack_entropy_hash),
            outgoing_ack()->received_info.entropy_hash);
  ProcessDataPacket(25, 1, kEntropyFlag);
  EXPECT_EQ((kRandomEntropyHash + ack_entropy_hash + (1 << (25 % 8))),
            outgoing_ack()->received_info.entropy_hash);
}

TEST_P(QuicConnectionTest, EntropyCalculationForTruncatedAck) {
  EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(AtLeast(1));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacketEntropyHash entropy[51];
  entropy[0] = 0;
  for (int i = 1; i < 51; ++i) {
    bool should_send = i % 10 != 1;
    bool entropy_flag = (i & (i - 1)) != 0;
    if (!should_send) {
      entropy[i] = entropy[i - 1];
      continue;
    }
    if (entropy_flag) {
      entropy[i] = entropy[i - 1] ^ (1 << (i % 8));
    } else {
      entropy[i] = entropy[i - 1];
    }
    ProcessDataPacket(i, 1, entropy_flag);
  }
  for (int i = 1; i < 50; ++i) {
    EXPECT_EQ(entropy[i], QuicConnectionPeer::ReceivedEntropyHash(
        &connection_, i));
  }
}

TEST_P(QuicConnectionTest, CheckSentEntropyHash) {
  peer_creator_.set_sequence_number(1);
  SequenceNumberSet missing_packets;
  QuicPacketEntropyHash entropy_hash = 0;
  QuicPacketSequenceNumber max_sequence_number = 51;
  for (QuicPacketSequenceNumber i = 1; i <= max_sequence_number; ++i) {
    bool is_missing = i % 10 != 0;
    bool entropy_flag = (i & (i - 1)) != 0;
    QuicPacketEntropyHash packet_entropy_hash = 0;
    if (entropy_flag) {
      packet_entropy_hash = 1 << (i % 8);
    }
    QuicPacket* packet = ConstructDataPacket(i, 0, entropy_flag);
    connection_.SendPacket(
        ENCRYPTION_NONE, i, packet, packet_entropy_hash,
        HAS_RETRANSMITTABLE_DATA);

    if (is_missing)  {
      missing_packets.insert(i);
      continue;
    }

    entropy_hash ^= packet_entropy_hash;
  }
  EXPECT_TRUE(QuicConnectionPeer::IsValidEntropy(
      &connection_, max_sequence_number, missing_packets, entropy_hash))
      << "";
}

TEST_P(QuicConnectionTest, ServerSendsVersionNegotiationPacket) {
  connection_.SetSupportedVersions(QuicSupportedVersions());
  framer_.set_version_for_tests(QUIC_VERSION_UNSUPPORTED);

  QuicPacketHeader header;
  header.public_header.connection_id = connection_id_;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = true;
  header.entropy_flag = false;
  header.fec_flag = false;
  header.packet_sequence_number = 12;
  header.fec_group = 0;

  QuicFrames frames;
  QuicFrame frame(&frame1_);
  frames.push_back(frame);
  scoped_ptr<QuicPacket> packet(
      BuildUnsizedDataPacket(&framer_, header, frames).packet);
  scoped_ptr<QuicEncryptedPacket> encrypted(
      framer_.EncryptPacket(ENCRYPTION_NONE, 12, *packet));

  framer_.set_version(version());
  connection_.set_is_server(true);
  connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
  EXPECT_TRUE(writer_->version_negotiation_packet() != NULL);

  size_t num_versions = arraysize(kSupportedQuicVersions);
  ASSERT_EQ(num_versions,
            writer_->version_negotiation_packet()->versions.size());

  // We expect all versions in kSupportedQuicVersions to be
  // included in the packet.
  for (size_t i = 0; i < num_versions; ++i) {
    EXPECT_EQ(kSupportedQuicVersions[i],
              writer_->version_negotiation_packet()->versions[i]);
  }
}

TEST_P(QuicConnectionTest, ServerSendsVersionNegotiationPacketSocketBlocked) {
  connection_.SetSupportedVersions(QuicSupportedVersions());
  framer_.set_version_for_tests(QUIC_VERSION_UNSUPPORTED);

  QuicPacketHeader header;
  header.public_header.connection_id = connection_id_;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = true;
  header.entropy_flag = false;
  header.fec_flag = false;
  header.packet_sequence_number = 12;
  header.fec_group = 0;

  QuicFrames frames;
  QuicFrame frame(&frame1_);
  frames.push_back(frame);
  scoped_ptr<QuicPacket> packet(
      BuildUnsizedDataPacket(&framer_, header, frames).packet);
  scoped_ptr<QuicEncryptedPacket> encrypted(
      framer_.EncryptPacket(ENCRYPTION_NONE, 12, *packet));

  framer_.set_version(version());
  connection_.set_is_server(true);
  BlockOnNextWrite();
  connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
  EXPECT_EQ(0u, writer_->last_packet_size());
  EXPECT_TRUE(connection_.HasQueuedData());

  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_TRUE(writer_->version_negotiation_packet() != NULL);

  size_t num_versions = arraysize(kSupportedQuicVersions);
  ASSERT_EQ(num_versions,
            writer_->version_negotiation_packet()->versions.size());

  // We expect all versions in kSupportedQuicVersions to be
  // included in the packet.
  for (size_t i = 0; i < num_versions; ++i) {
    EXPECT_EQ(kSupportedQuicVersions[i],
              writer_->version_negotiation_packet()->versions[i]);
  }
}

TEST_P(QuicConnectionTest,
       ServerSendsVersionNegotiationPacketSocketBlockedDataBuffered) {
  connection_.SetSupportedVersions(QuicSupportedVersions());
  framer_.set_version_for_tests(QUIC_VERSION_UNSUPPORTED);

  QuicPacketHeader header;
  header.public_header.connection_id = connection_id_;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = true;
  header.entropy_flag = false;
  header.fec_flag = false;
  header.packet_sequence_number = 12;
  header.fec_group = 0;

  QuicFrames frames;
  QuicFrame frame(&frame1_);
  frames.push_back(frame);
  scoped_ptr<QuicPacket> packet(
      BuildUnsizedDataPacket(&framer_, header, frames).packet);
  scoped_ptr<QuicEncryptedPacket> encrypted(
      framer_.EncryptPacket(ENCRYPTION_NONE, 12, *packet));

  framer_.set_version(version());
  connection_.set_is_server(true);
  BlockOnNextWrite();
  writer_->set_is_write_blocked_data_buffered(true);
  connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
  EXPECT_EQ(0u, writer_->last_packet_size());
  EXPECT_FALSE(connection_.HasQueuedData());
}

TEST_P(QuicConnectionTest, ClientHandlesVersionNegotiation) {
  // Start out with some unsupported version.
  QuicConnectionPeer::GetFramer(&connection_)->set_version_for_tests(
      QUIC_VERSION_UNSUPPORTED);

  QuicPacketHeader header;
  header.public_header.connection_id = connection_id_;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = true;
  header.entropy_flag = false;
  header.fec_flag = false;
  header.packet_sequence_number = 12;
  header.fec_group = 0;

  QuicVersionVector supported_versions;
  for (size_t i = 0; i < arraysize(kSupportedQuicVersions); ++i) {
    supported_versions.push_back(kSupportedQuicVersions[i]);
  }

  // Send a version negotiation packet.
  scoped_ptr<QuicEncryptedPacket> encrypted(
      framer_.BuildVersionNegotiationPacket(
          header.public_header, supported_versions));
  connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);

  // Now force another packet.  The connection should transition into
  // NEGOTIATED_VERSION state and tell the packet creator to StopSendingVersion.
  header.public_header.version_flag = false;
  QuicFrames frames;
  QuicFrame frame(&frame1_);
  frames.push_back(frame);
  scoped_ptr<QuicPacket> packet(
      BuildUnsizedDataPacket(&framer_, header, frames).packet);
  encrypted.reset(framer_.EncryptPacket(ENCRYPTION_NONE, 12, *packet));
  EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);

  ASSERT_FALSE(QuicPacketCreatorPeer::SendVersionInPacket(
      QuicConnectionPeer::GetPacketCreator(&connection_)));
}

TEST_P(QuicConnectionTest, BadVersionNegotiation) {
  QuicPacketHeader header;
  header.public_header.connection_id = connection_id_;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = true;
  header.entropy_flag = false;
  header.fec_flag = false;
  header.packet_sequence_number = 12;
  header.fec_group = 0;

  QuicVersionVector supported_versions;
  for (size_t i = 0; i < arraysize(kSupportedQuicVersions); ++i) {
    supported_versions.push_back(kSupportedQuicVersions[i]);
  }

  // Send a version negotiation packet with the version the client started with.
  // It should be rejected.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(QUIC_INVALID_VERSION_NEGOTIATION_PACKET,
                                 false));
  scoped_ptr<QuicEncryptedPacket> encrypted(
      framer_.BuildVersionNegotiationPacket(
          header.public_header, supported_versions));
  connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
}

TEST_P(QuicConnectionTest, CheckSendStats) {
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendStreamDataWithString(3, "first", 0, !kFin, NULL);
  size_t first_packet_size = writer_->last_packet_size();

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendStreamDataWithString(5, "second", 0, !kFin, NULL);
  size_t second_packet_size = writer_->last_packet_size();

  // 2 retransmissions due to rto, 1 due to explicit nack.
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(3);

  // Retransmit due to RTO.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(10));
  connection_.GetRetransmissionAlarm()->Fire();

  // Retransmit due to explicit nacks.
  QuicAckFrame nack_three = InitAckFrame(4, 0);
  NackPacket(3, &nack_three);
  NackPacket(1, &nack_three);
  SequenceNumberSet lost_packets;
  lost_packets.insert(1);
  lost_packets.insert(3);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  EXPECT_CALL(visitor_, OnCanWrite()).Times(2);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessAckPacket(&nack_three);

  EXPECT_CALL(*send_algorithm_, BandwidthEstimate()).WillOnce(
      Return(QuicBandwidth::Zero()));

  const QuicConnectionStats& stats = connection_.GetStats();
  EXPECT_EQ(3 * first_packet_size + 2 * second_packet_size - kQuicVersionSize,
            stats.bytes_sent);
  EXPECT_EQ(5u, stats.packets_sent);
  EXPECT_EQ(2 * first_packet_size + second_packet_size - kQuicVersionSize,
            stats.bytes_retransmitted);
  EXPECT_EQ(3u, stats.packets_retransmitted);
  EXPECT_EQ(1u, stats.rto_count);
}

TEST_P(QuicConnectionTest, CheckReceiveStats) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  size_t received_bytes = 0;
  received_bytes += ProcessFecProtectedPacket(1, false, !kEntropyFlag);
  received_bytes += ProcessFecProtectedPacket(3, false, !kEntropyFlag);
  // Should be counted against dropped packets.
  received_bytes += ProcessDataPacket(3, 1, !kEntropyFlag);
  received_bytes += ProcessFecPacket(4, 1, true, !kEntropyFlag, NULL);

  EXPECT_CALL(*send_algorithm_, BandwidthEstimate()).WillOnce(
      Return(QuicBandwidth::Zero()));

  const QuicConnectionStats& stats = connection_.GetStats();
  EXPECT_EQ(received_bytes, stats.bytes_received);
  EXPECT_EQ(4u, stats.packets_received);

  EXPECT_EQ(1u, stats.packets_revived);
  EXPECT_EQ(1u, stats.packets_dropped);
}

TEST_P(QuicConnectionTest, TestFecGroupLimits) {
  // Create and return a group for 1.
  ASSERT_TRUE(QuicConnectionPeer::GetFecGroup(&connection_, 1) != NULL);

  // Create and return a group for 2.
  ASSERT_TRUE(QuicConnectionPeer::GetFecGroup(&connection_, 2) != NULL);

  // Create and return a group for 4.  This should remove 1 but not 2.
  ASSERT_TRUE(QuicConnectionPeer::GetFecGroup(&connection_, 4) != NULL);
  ASSERT_TRUE(QuicConnectionPeer::GetFecGroup(&connection_, 1) == NULL);
  ASSERT_TRUE(QuicConnectionPeer::GetFecGroup(&connection_, 2) != NULL);

  // Create and return a group for 3.  This will kill off 2.
  ASSERT_TRUE(QuicConnectionPeer::GetFecGroup(&connection_, 3) != NULL);
  ASSERT_TRUE(QuicConnectionPeer::GetFecGroup(&connection_, 2) == NULL);

  // Verify that adding 5 kills off 3, despite 4 being created before 3.
  ASSERT_TRUE(QuicConnectionPeer::GetFecGroup(&connection_, 5) != NULL);
  ASSERT_TRUE(QuicConnectionPeer::GetFecGroup(&connection_, 4) != NULL);
  ASSERT_TRUE(QuicConnectionPeer::GetFecGroup(&connection_, 3) == NULL);
}

TEST_P(QuicConnectionTest, ProcessFramesIfPacketClosedConnection) {
  // Construct a packet with stream frame and connection close frame.
  header_.public_header.connection_id = connection_id_;
  header_.packet_sequence_number = 1;
  header_.public_header.reset_flag = false;
  header_.public_header.version_flag = false;
  header_.entropy_flag = false;
  header_.fec_flag = false;
  header_.fec_group = 0;

  QuicConnectionCloseFrame qccf;
  qccf.error_code = QUIC_PEER_GOING_AWAY;
  QuicFrame close_frame(&qccf);
  QuicFrame stream_frame(&frame1_);

  QuicFrames frames;
  frames.push_back(stream_frame);
  frames.push_back(close_frame);
  scoped_ptr<QuicPacket> packet(
      BuildUnsizedDataPacket(&framer_, header_, frames).packet);
  EXPECT_TRUE(NULL != packet.get());
  scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(
      ENCRYPTION_NONE, 1, *packet));

  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PEER_GOING_AWAY, true));
  EXPECT_CALL(visitor_, OnStreamFrames(_)).Times(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
}

TEST_P(QuicConnectionTest, SelectMutualVersion) {
  connection_.SetSupportedVersions(QuicSupportedVersions());
  // Set the connection to speak the lowest quic version.
  connection_.set_version(QuicVersionMin());
  EXPECT_EQ(QuicVersionMin(), connection_.version());

  // Pass in available versions which includes a higher mutually supported
  // version.  The higher mutually supported version should be selected.
  QuicVersionVector supported_versions;
  for (size_t i = 0; i < arraysize(kSupportedQuicVersions); ++i) {
    supported_versions.push_back(kSupportedQuicVersions[i]);
  }
  EXPECT_TRUE(connection_.SelectMutualVersion(supported_versions));
  EXPECT_EQ(QuicVersionMax(), connection_.version());

  // Expect that the lowest version is selected.
  // Ensure the lowest supported version is less than the max, unless they're
  // the same.
  EXPECT_LE(QuicVersionMin(), QuicVersionMax());
  QuicVersionVector lowest_version_vector;
  lowest_version_vector.push_back(QuicVersionMin());
  EXPECT_TRUE(connection_.SelectMutualVersion(lowest_version_vector));
  EXPECT_EQ(QuicVersionMin(), connection_.version());

  // Shouldn't be able to find a mutually supported version.
  QuicVersionVector unsupported_version;
  unsupported_version.push_back(QUIC_VERSION_UNSUPPORTED);
  EXPECT_FALSE(connection_.SelectMutualVersion(unsupported_version));
}

TEST_P(QuicConnectionTest, ConnectionCloseWhenWritable) {
  EXPECT_FALSE(writer_->IsWriteBlocked());

  // Send a packet.
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, NULL);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  TriggerConnectionClose();
  EXPECT_EQ(2u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, ConnectionCloseGettingWriteBlocked) {
  BlockOnNextWrite();
  TriggerConnectionClose();
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_TRUE(writer_->IsWriteBlocked());
}

TEST_P(QuicConnectionTest, ConnectionCloseWhenWriteBlocked) {
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, NULL);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_TRUE(writer_->IsWriteBlocked());
  TriggerConnectionClose();
  EXPECT_EQ(1u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, AckNotifierTriggerCallback) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Create a delegate which we expect to be called.
  scoped_refptr<MockAckNotifierDelegate> delegate(new MockAckNotifierDelegate);
  EXPECT_CALL(*delegate, OnAckNotification(_, _, _, _, _)).Times(1);

  // Send some data, which will register the delegate to be notified.
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, delegate.get());

  // Process an ACK from the server which should trigger the callback.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicAckFrame frame = InitAckFrame(1, 0);
  ProcessAckPacket(&frame);
}

TEST_P(QuicConnectionTest, AckNotifierFailToTriggerCallback) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Create a delegate which we don't expect to be called.
  scoped_refptr<MockAckNotifierDelegate> delegate(new MockAckNotifierDelegate);
  EXPECT_CALL(*delegate, OnAckNotification(_, _, _, _, _)).Times(0);

  // Send some data, which will register the delegate to be notified. This will
  // not be ACKed and so the delegate should never be called.
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, delegate.get());

  // Send some other data which we will ACK.
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, NULL);
  connection_.SendStreamDataWithString(1, "bar", 0, !kFin, NULL);

  // Now we receive ACK for packets 2 and 3, but importantly missing packet 1
  // which we registered to be notified about.
  QuicAckFrame frame = InitAckFrame(3, 0);
  NackPacket(1, &frame);
  SequenceNumberSet lost_packets;
  lost_packets.insert(1);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  ProcessAckPacket(&frame);
}

TEST_P(QuicConnectionTest, AckNotifierCallbackAfterRetransmission) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Create a delegate which we expect to be called.
  scoped_refptr<MockAckNotifierDelegate> delegate(new MockAckNotifierDelegate);
  EXPECT_CALL(*delegate, OnAckNotification(_, _, _, _, _)).Times(1);

  // Send four packets, and register to be notified on ACK of packet 2.
  connection_.SendStreamDataWithString(3, "foo", 0, !kFin, NULL);
  connection_.SendStreamDataWithString(3, "bar", 0, !kFin, delegate.get());
  connection_.SendStreamDataWithString(3, "baz", 0, !kFin, NULL);
  connection_.SendStreamDataWithString(3, "qux", 0, !kFin, NULL);

  // Now we receive ACK for packets 1, 3, and 4 and lose 2.
  QuicAckFrame frame = InitAckFrame(4, 0);
  NackPacket(2, &frame);
  SequenceNumberSet lost_packets;
  lost_packets.insert(2);
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  ProcessAckPacket(&frame);

  // Now we get an ACK for packet 5 (retransmitted packet 2), which should
  // trigger the callback.
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillRepeatedly(Return(SequenceNumberSet()));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicAckFrame second_ack_frame = InitAckFrame(5, 0);
  ProcessAckPacket(&second_ack_frame);
}

// AckNotifierCallback is triggered by the ack of a packet that timed
// out and was retransmitted, even though the retransmission has a
// different sequence number.
TEST_P(QuicConnectionTest, AckNotifierCallbackForAckAfterRTO) {
  InSequence s;

  // Create a delegate which we expect to be called.
  scoped_refptr<MockAckNotifierDelegate> delegate(
      new StrictMock<MockAckNotifierDelegate>);

  QuicTime default_retransmission_time = clock_.ApproximateNow().Add(
      DefaultRetransmissionTime());
  connection_.SendStreamDataWithString(3, "foo", 0, !kFin, delegate.get());
  EXPECT_EQ(1u, outgoing_ack()->sent_info.least_unacked);

  EXPECT_EQ(1u, writer_->header().packet_sequence_number);
  EXPECT_EQ(default_retransmission_time,
            connection_.GetRetransmissionAlarm()->deadline());
  // Simulate the retransmission alarm firing.
  clock_.AdvanceTime(DefaultRetransmissionTime());
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 2u, _, _));
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(2u, writer_->header().packet_sequence_number);
  // We do not raise the high water mark yet.
  EXPECT_EQ(1u, outgoing_ack()->sent_info.least_unacked);

  // Ack the original packet.
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*delegate, OnAckNotification(1, _, 1, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicAckFrame ack_frame = InitAckFrame(1, 0);
  ProcessAckPacket(&ack_frame);

  // Delegate is not notified again when the retransmit is acked.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicAckFrame second_ack_frame = InitAckFrame(2, 0);
  ProcessAckPacket(&second_ack_frame);
}

// AckNotifierCallback is triggered by the ack of a packet that was
// previously nacked, even though the retransmission has a different
// sequence number.
TEST_P(QuicConnectionTest, AckNotifierCallbackForAckOfNackedPacket) {
  InSequence s;

  // Create a delegate which we expect to be called.
  scoped_refptr<MockAckNotifierDelegate> delegate(
      new StrictMock<MockAckNotifierDelegate>);

  // Send four packets, and register to be notified on ACK of packet 2.
  connection_.SendStreamDataWithString(3, "foo", 0, !kFin, NULL);
  connection_.SendStreamDataWithString(3, "bar", 0, !kFin, delegate.get());
  connection_.SendStreamDataWithString(3, "baz", 0, !kFin, NULL);
  connection_.SendStreamDataWithString(3, "qux", 0, !kFin, NULL);

  // Now we receive ACK for packets 1, 3, and 4 and lose 2.
  QuicAckFrame frame = InitAckFrame(4, 0);
  NackPacket(2, &frame);
  SequenceNumberSet lost_packets;
  lost_packets.insert(2);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  ProcessAckPacket(&frame);

  // Now we get an ACK for packet 2, which was previously nacked.
  SequenceNumberSet no_lost_packets;
  EXPECT_CALL(*delegate, OnAckNotification(1, _, 1, _, _));
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(no_lost_packets));
  QuicAckFrame second_ack_frame = InitAckFrame(4, 0);
  ProcessAckPacket(&second_ack_frame);

  // Verify that the delegate is not notified again when the
  // retransmit is acked.
  EXPECT_CALL(*loss_algorithm_, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(no_lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicAckFrame third_ack_frame = InitAckFrame(5, 0);
  ProcessAckPacket(&third_ack_frame);
}

TEST_P(QuicConnectionTest, AckNotifierFECTriggerCallback) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Create a delegate which we expect to be called.
  scoped_refptr<MockAckNotifierDelegate> delegate(
      new MockAckNotifierDelegate);
  EXPECT_CALL(*delegate, OnAckNotification(_, _, _, _, _)).Times(1);

  // Send some data, which will register the delegate to be notified.
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, delegate.get());
  connection_.SendStreamDataWithString(2, "bar", 0, !kFin, NULL);

  // Process an ACK from the server with a revived packet, which should trigger
  // the callback.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));
  QuicAckFrame frame = InitAckFrame(2, 0);
  NackPacket(1, &frame);
  frame.received_info.revived_packets.insert(1);
  ProcessAckPacket(&frame);
  // If the ack is processed again, the notifier should not be called again.
  ProcessAckPacket(&frame);
}

TEST_P(QuicConnectionTest, AckNotifierCallbackAfterFECRecovery) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnCanWrite());

  // Create a delegate which we expect to be called.
  scoped_refptr<MockAckNotifierDelegate> delegate(new MockAckNotifierDelegate);
  EXPECT_CALL(*delegate, OnAckNotification(_, _, _, _, _)).Times(1);

  // Expect ACKs for 1 packet.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _));

  // Send one packet, and register to be notified on ACK.
  connection_.SendStreamDataWithString(1, "foo", 0, !kFin, delegate.get());

  // Ack packet gets dropped, but we receive an FEC packet that covers it.
  // Should recover the Ack packet and trigger the notification callback.
  QuicFrames frames;

  QuicAckFrame ack_frame = InitAckFrame(1, 0);
  frames.push_back(QuicFrame(&ack_frame));

  // Dummy stream frame to satisfy expectations set elsewhere.
  frames.push_back(QuicFrame(&frame1_));

  QuicPacketHeader ack_header;
  ack_header.public_header.connection_id = connection_id_;
  ack_header.public_header.reset_flag = false;
  ack_header.public_header.version_flag = false;
  ack_header.entropy_flag = !kEntropyFlag;
  ack_header.fec_flag = true;
  ack_header.packet_sequence_number = 1;
  ack_header.is_in_fec_group = IN_FEC_GROUP;
  ack_header.fec_group = 1;

  QuicPacket* packet =
      BuildUnsizedDataPacket(&framer_, ack_header, frames).packet;

  // Take the packet which contains the ACK frame, and construct and deliver an
  // FEC packet which allows the ACK packet to be recovered.
  ProcessFecPacket(2, 1, true, !kEntropyFlag, packet);
}

class MockQuicConnectionDebugVisitor
    : public QuicConnectionDebugVisitor {
 public:
  MOCK_METHOD1(OnFrameAddedToPacket,
               void(const QuicFrame&));

  MOCK_METHOD5(OnPacketSent,
               void(QuicPacketSequenceNumber,
                    EncryptionLevel,
                    TransmissionType,
                    const QuicEncryptedPacket&,
                    WriteResult));

  MOCK_METHOD2(OnPacketRetransmitted,
               void(QuicPacketSequenceNumber,
                    QuicPacketSequenceNumber));

  MOCK_METHOD3(OnPacketReceived,
               void(const IPEndPoint&,
                    const IPEndPoint&,
                    const QuicEncryptedPacket&));

  MOCK_METHOD1(OnProtocolVersionMismatch,
               void(QuicVersion));

  MOCK_METHOD1(OnPacketHeader,
               void(const QuicPacketHeader& header));

  MOCK_METHOD1(OnStreamFrame,
               void(const QuicStreamFrame&));

  MOCK_METHOD1(OnAckFrame,
               void(const QuicAckFrame& frame));

  MOCK_METHOD1(OnCongestionFeedbackFrame,
               void(const QuicCongestionFeedbackFrame&));

  MOCK_METHOD1(OnStopWaitingFrame,
               void(const QuicStopWaitingFrame&));

  MOCK_METHOD1(OnRstStreamFrame,
               void(const QuicRstStreamFrame&));

  MOCK_METHOD1(OnConnectionCloseFrame,
               void(const QuicConnectionCloseFrame&));

  MOCK_METHOD1(OnPublicResetPacket,
               void(const QuicPublicResetPacket&));

  MOCK_METHOD1(OnVersionNegotiationPacket,
               void(const QuicVersionNegotiationPacket&));

  MOCK_METHOD2(OnRevivedPacket,
               void(const QuicPacketHeader&, StringPiece payload));
};

TEST_P(QuicConnectionTest, OnPacketHeaderDebugVisitor) {
  QuicPacketHeader header;

  scoped_ptr<MockQuicConnectionDebugVisitor>
      debug_visitor(new StrictMock<MockQuicConnectionDebugVisitor>);
  connection_.set_debug_visitor(debug_visitor.get());
  EXPECT_CALL(*debug_visitor, OnPacketHeader(Ref(header))).Times(1);
  connection_.OnPacketHeader(header);
}

TEST_P(QuicConnectionTest, Pacing) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_pacing, true);

  TestConnection server(connection_id_, IPEndPoint(), helper_.get(),
                        writer_.get(), true, version());
  TestConnection client(connection_id_, IPEndPoint(), helper_.get(),
                        writer_.get(), false, version());
  EXPECT_TRUE(client.sent_packet_manager().using_pacing());
  EXPECT_FALSE(server.sent_packet_manager().using_pacing());
}

TEST_P(QuicConnectionTest, ControlFramesInstigateAcks) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Send a WINDOW_UPDATE frame.
  QuicWindowUpdateFrame window_update;
  window_update.stream_id = 3;
  window_update.byte_offset = 1234;
  EXPECT_CALL(visitor_, OnWindowUpdateFrames(_));
  ProcessFramePacket(QuicFrame(&window_update));

  // Ensure that this has caused the ACK alarm to be set.
  QuicAlarm* ack_alarm = QuicConnectionPeer::GetAckAlarm(&connection_);
  EXPECT_TRUE(ack_alarm->IsSet());

  // Cancel alarm, and try again with BLOCKED frame.
  ack_alarm->Cancel();
  QuicBlockedFrame blocked;
  blocked.stream_id = 3;
  EXPECT_CALL(visitor_, OnBlockedFrames(_));
  ProcessFramePacket(QuicFrame(&blocked));
  EXPECT_TRUE(ack_alarm->IsSet());
}

}  // namespace
}  // namespace test
}  // namespace net
