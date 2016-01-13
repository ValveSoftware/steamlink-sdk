// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_dispatcher.h"

#include <string>

#include "base/strings/string_piece.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_packet_writer_wrapper.h"
#include "net/tools/quic/quic_time_wait_list_manager.h"
#include "net/tools/quic/test_tools/quic_dispatcher_peer.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using net::EpollServer;
using net::test::ConstructEncryptedPacket;
using net::test::MockSession;
using net::test::ValueRestore;
using net::tools::test::MockConnection;
using std::make_pair;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::WithoutArgs;
using testing::_;

namespace net {
namespace tools {
namespace test {
namespace {

class TestDispatcher : public QuicDispatcher {
 public:
  explicit TestDispatcher(const QuicConfig& config,
                          const QuicCryptoServerConfig& crypto_config,
                          EpollServer* eps)
      : QuicDispatcher(config,
                       crypto_config,
                       QuicSupportedVersions(),
                       eps) {
  }

  MOCK_METHOD3(CreateQuicSession, QuicSession*(
      QuicConnectionId connection_id,
      const IPEndPoint& server_address,
      const IPEndPoint& client_address));
  using QuicDispatcher::write_blocked_list;

  using QuicDispatcher::current_server_address;
  using QuicDispatcher::current_client_address;
};

// A Connection class which unregisters the session from the dispatcher
// when sending connection close.
// It'd be slightly more realistic to do this from the Session but it would
// involve a lot more mocking.
class MockServerConnection : public MockConnection {
 public:
  MockServerConnection(QuicConnectionId connection_id,
                       QuicDispatcher* dispatcher)
      : MockConnection(connection_id, true),
        dispatcher_(dispatcher) {}

  void UnregisterOnConnectionClosed() {
    LOG(ERROR) << "Unregistering " << connection_id();
    dispatcher_->OnConnectionClosed(connection_id(), QUIC_NO_ERROR);
  }
 private:
  QuicDispatcher* dispatcher_;
};

QuicSession* CreateSession(QuicDispatcher* dispatcher,
                           QuicConnectionId connection_id,
                           const IPEndPoint& client_address,
                           MockSession** session) {
  MockServerConnection* connection =
      new MockServerConnection(connection_id, dispatcher);
  *session = new MockSession(connection);
  ON_CALL(*connection, SendConnectionClose(_)).WillByDefault(
      WithoutArgs(Invoke(
          connection, &MockServerConnection::UnregisterOnConnectionClosed)));
  EXPECT_CALL(*reinterpret_cast<MockConnection*>((*session)->connection()),
              ProcessUdpPacket(_, client_address, _));

  return *session;
}

class QuicDispatcherTest : public ::testing::Test {
 public:
  QuicDispatcherTest()
      : crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance()),
        dispatcher_(config_, crypto_config_, &eps_),
        session1_(NULL),
        session2_(NULL) {
    dispatcher_.Initialize(1);
  }

  virtual ~QuicDispatcherTest() {}

  MockConnection* connection1() {
    return reinterpret_cast<MockConnection*>(session1_->connection());
  }

  MockConnection* connection2() {
    return reinterpret_cast<MockConnection*>(session2_->connection());
  }

  void ProcessPacket(IPEndPoint client_address,
                     QuicConnectionId connection_id,
                     bool has_version_flag,
                     const string& data) {
    scoped_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
        connection_id, has_version_flag, false, 1, data));
    data_ = string(packet->data(), packet->length());
    dispatcher_.ProcessPacket(server_address_, client_address, *packet);
  }

  void ValidatePacket(const QuicEncryptedPacket& packet) {
    EXPECT_EQ(data_.length(), packet.AsStringPiece().length());
    EXPECT_EQ(data_, packet.AsStringPiece());
  }

  EpollServer eps_;
  QuicConfig config_;
  QuicCryptoServerConfig crypto_config_;
  IPEndPoint server_address_;
  TestDispatcher dispatcher_;
  MockSession* session1_;
  MockSession* session2_;
  string data_;
};

TEST_F(QuicDispatcherTest, ProcessPackets) {
  IPEndPoint client_address(net::test::Loopback4(), 1);
  IPAddressNumber any4;
  CHECK(net::ParseIPLiteralToNumber("0.0.0.0", &any4));
  server_address_ = IPEndPoint(any4, 5);

  EXPECT_CALL(dispatcher_, CreateQuicSession(1, _, client_address))
      .WillOnce(testing::Return(CreateSession(
          &dispatcher_, 1, client_address, &session1_)));
  ProcessPacket(client_address, 1, true, "foo");
  EXPECT_EQ(client_address, dispatcher_.current_client_address());
  EXPECT_EQ(server_address_, dispatcher_.current_server_address());


  EXPECT_CALL(dispatcher_, CreateQuicSession(2, _, client_address))
      .WillOnce(testing::Return(CreateSession(
                    &dispatcher_, 2, client_address, &session2_)));
  ProcessPacket(client_address, 2, true, "bar");

  EXPECT_CALL(*reinterpret_cast<MockConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _)).Times(1).
      WillOnce(testing::WithArgs<2>(Invoke(
          this, &QuicDispatcherTest::ValidatePacket)));
  ProcessPacket(client_address, 1, false, "eep");
}

TEST_F(QuicDispatcherTest, Shutdown) {
  IPEndPoint client_address(net::test::Loopback4(), 1);

  EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, client_address))
      .WillOnce(testing::Return(CreateSession(
                    &dispatcher_, 1, client_address, &session1_)));

  ProcessPacket(client_address, 1, true, "foo");

  EXPECT_CALL(*reinterpret_cast<MockConnection*>(session1_->connection()),
              SendConnectionClose(QUIC_PEER_GOING_AWAY));

  dispatcher_.Shutdown();
}

class MockTimeWaitListManager : public QuicTimeWaitListManager {
 public:
  MockTimeWaitListManager(QuicPacketWriter* writer,
                          QuicServerSessionVisitor* visitor,
                          EpollServer* eps)
      : QuicTimeWaitListManager(writer, visitor, eps, QuicSupportedVersions()) {
  }

  MOCK_METHOD5(ProcessPacket, void(const IPEndPoint& server_address,
                                   const IPEndPoint& client_address,
                                   QuicConnectionId connection_id,
                                   QuicPacketSequenceNumber sequence_number,
                                   const QuicEncryptedPacket& packet));
};

TEST_F(QuicDispatcherTest, TimeWaitListManager) {
  MockTimeWaitListManager* time_wait_list_manager =
      new MockTimeWaitListManager(
          QuicDispatcherPeer::GetWriter(&dispatcher_), &dispatcher_, &eps_);
  // dispatcher takes the ownership of time_wait_list_manager.
  QuicDispatcherPeer::SetTimeWaitListManager(&dispatcher_,
                                             time_wait_list_manager);
  // Create a new session.
  IPEndPoint client_address(net::test::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  EXPECT_CALL(dispatcher_, CreateQuicSession(connection_id, _, client_address))
      .WillOnce(testing::Return(CreateSession(
                    &dispatcher_, connection_id, client_address, &session1_)));
  ProcessPacket(client_address, connection_id, true, "foo");

  // Close the connection by sending public reset packet.
  QuicPublicResetPacket packet;
  packet.public_header.connection_id = connection_id;
  packet.public_header.reset_flag = true;
  packet.public_header.version_flag = false;
  packet.rejected_sequence_number = 19191;
  packet.nonce_proof = 132232;
  scoped_ptr<QuicEncryptedPacket> encrypted(
      QuicFramer::BuildPublicResetPacket(packet));
  EXPECT_CALL(*session1_, OnConnectionClosed(QUIC_PUBLIC_RESET, true)).Times(1)
      .WillOnce(WithoutArgs(Invoke(
          reinterpret_cast<MockServerConnection*>(session1_->connection()),
          &MockServerConnection::UnregisterOnConnectionClosed)));
  EXPECT_CALL(*reinterpret_cast<MockConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(Invoke(
          reinterpret_cast<MockConnection*>(session1_->connection()),
          &MockConnection::ReallyProcessUdpPacket));
  dispatcher_.ProcessPacket(IPEndPoint(), client_address, *encrypted);
  EXPECT_TRUE(time_wait_list_manager->IsConnectionIdInTimeWait(connection_id));

  // Dispatcher forwards subsequent packets for this connection_id to the time
  // wait list manager.
  EXPECT_CALL(*time_wait_list_manager,
              ProcessPacket(_, _, connection_id, _, _)).Times(1);
  ProcessPacket(client_address, connection_id, true, "foo");
}

TEST_F(QuicDispatcherTest, StrayPacketToTimeWaitListManager) {
  MockTimeWaitListManager* time_wait_list_manager =
      new MockTimeWaitListManager(
          QuicDispatcherPeer::GetWriter(&dispatcher_), &dispatcher_, &eps_);
  // dispatcher takes the ownership of time_wait_list_manager.
  QuicDispatcherPeer::SetTimeWaitListManager(&dispatcher_,
                                             time_wait_list_manager);

  IPEndPoint client_address(net::test::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  // Dispatcher forwards all packets for this connection_id to the time wait
  // list manager.
  EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager,
              ProcessPacket(_, _, connection_id, _, _)).Times(1);
  string data = "foo";
  ProcessPacket(client_address, connection_id, false, "foo");
}

TEST(QuicDispatcherFlowControlTest, NoNewVersion17ConnectionsIfFlagDisabled) {
  // If FLAGS_enable_quic_stream_flow_control_2 is disabled
  // then the dispatcher should stop creating connections that support
  // QUIC_VERSION_17 (existing connections will stay alive).
  // TODO(rjshade): Remove once
  // FLAGS_enable_quic_stream_flow_control_2 is removed.

  EpollServer eps;
  QuicConfig config;
  QuicCryptoServerConfig server_config(QuicCryptoServerConfig::TESTING,
                                       QuicRandom::GetInstance());
  IPEndPoint client(net::test::Loopback4(), 1);
  IPEndPoint server(net::test::Loopback4(), 1);
  QuicConnectionId kCID = 1234;

  QuicVersion kTestQuicVersions[] = {QUIC_VERSION_17,
                                     QUIC_VERSION_16,
                                     QUIC_VERSION_15};
  QuicVersionVector kTestVersions;
  for (size_t i = 0; i < arraysize(kTestQuicVersions); ++i) {
    kTestVersions.push_back(kTestQuicVersions[i]);
  }

  QuicDispatcher dispatcher(config, server_config, kTestVersions, &eps);
  dispatcher.Initialize(0);

  // When flag is enabled, new connections should support QUIC_VERSION_17.
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control_2, true);
  scoped_ptr<QuicConnection> connection_1(
      QuicDispatcherPeer::CreateQuicConnection(&dispatcher, kCID, client,
                                               server));
  EXPECT_EQ(QUIC_VERSION_17, connection_1->version());


  // When flag is disabled, new connections should not support QUIC_VERSION_17.
  FLAGS_enable_quic_stream_flow_control_2 = false;
  scoped_ptr<QuicConnection> connection_2(
      QuicDispatcherPeer::CreateQuicConnection(&dispatcher, kCID, client,
                                               server));
  EXPECT_EQ(QUIC_VERSION_16, connection_2->version());
}

class BlockingWriter : public QuicPacketWriterWrapper {
 public:
  BlockingWriter() : write_blocked_(false) {}

  virtual bool IsWriteBlocked() const OVERRIDE { return write_blocked_; }
  virtual void SetWritable() OVERRIDE { write_blocked_ = false; }

  virtual WriteResult WritePacket(
      const char* buffer,
      size_t buf_len,
      const IPAddressNumber& self_client_address,
      const IPEndPoint& peer_client_address) OVERRIDE {
    if (write_blocked_) {
      return WriteResult(WRITE_STATUS_BLOCKED, EAGAIN);
    } else {
      return QuicPacketWriterWrapper::WritePacket(
          buffer, buf_len, self_client_address, peer_client_address);
    }
  }

  bool write_blocked_;
};

class QuicDispatcherWriteBlockedListTest : public QuicDispatcherTest {
 public:
  virtual void SetUp() {
    writer_ = new BlockingWriter;
    QuicDispatcherPeer::UseWriter(&dispatcher_, writer_);

    IPEndPoint client_address(net::test::Loopback4(), 1);

    EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, client_address))
        .WillOnce(testing::Return(CreateSession(
                      &dispatcher_, 1, client_address, &session1_)));
    ProcessPacket(client_address, 1, true, "foo");

    EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, client_address))
        .WillOnce(testing::Return(CreateSession(
                      &dispatcher_, 2, client_address, &session2_)));
    ProcessPacket(client_address, 2, true, "bar");

    blocked_list_ = dispatcher_.write_blocked_list();
  }

  virtual void TearDown() {
    EXPECT_CALL(*connection1(), SendConnectionClose(QUIC_PEER_GOING_AWAY));
    EXPECT_CALL(*connection2(), SendConnectionClose(QUIC_PEER_GOING_AWAY));
    dispatcher_.Shutdown();
  }

  void SetBlocked() {
    writer_->write_blocked_ = true;
  }

  void BlockConnection2() {
    writer_->write_blocked_ = true;
    dispatcher_.OnWriteBlocked(connection2());
  }

 protected:
  BlockingWriter* writer_;
  QuicDispatcher::WriteBlockedList* blocked_list_;
};

TEST_F(QuicDispatcherWriteBlockedListTest, BasicOnCanWrite) {
  // No OnCanWrite calls because no connections are blocked.
  dispatcher_.OnCanWrite();

  // Register connection 1 for events, and make sure it's notified.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_.OnCanWrite();

  // It should get only one notification.
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();
  EXPECT_FALSE(dispatcher_.HasPendingWrites());
}

TEST_F(QuicDispatcherWriteBlockedListTest, OnCanWriteOrder) {
  // Make sure we handle events in order.
  InSequence s;
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();

  // Check the other ordering.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection2());
  dispatcher_.OnWriteBlocked(connection1());
  EXPECT_CALL(*connection2(), OnCanWrite());
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_.OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, OnCanWriteRemove) {
  // Add and remove one connction.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();

  // Add and remove one connction and make sure it doesn't affect others.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection2());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();

  // Add it, remove it, and add it back and make sure things are OK.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  dispatcher_.OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_.OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, DoubleAdd) {
  // Make sure a double add does not necessitate a double remove.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();

  // Make sure a double add does not result in two OnCanWrite calls.
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_.OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, OnCanWriteHandleBlock) {
  // Finally make sure if we write block on a write call, we stop calling.
  InSequence s;
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite()).WillOnce(
      Invoke(this, &QuicDispatcherWriteBlockedListTest::SetBlocked));
  EXPECT_CALL(*connection2(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();

  // And we'll resume where we left off when we get another call.
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, LimitedWrites) {
  // Make sure we call both writers.  The first will register for more writing
  // but should not be immediately called due to limits.
  InSequence s;
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite()).WillOnce(
      Invoke(this, &QuicDispatcherWriteBlockedListTest::BlockConnection2));
  dispatcher_.OnCanWrite();
  EXPECT_TRUE(dispatcher_.HasPendingWrites());

  // Now call OnCanWrite again, and connection1 should get its second chance
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();
  EXPECT_FALSE(dispatcher_.HasPendingWrites());
}

TEST_F(QuicDispatcherWriteBlockedListTest, TestWriteLimits) {
  // Finally make sure if we write block on a write call, we stop calling.
  InSequence s;
  SetBlocked();
  dispatcher_.OnWriteBlocked(connection1());
  dispatcher_.OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite()).WillOnce(
      Invoke(this, &QuicDispatcherWriteBlockedListTest::SetBlocked));
  EXPECT_CALL(*connection2(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();
  EXPECT_TRUE(dispatcher_.HasPendingWrites());

  // And we'll resume where we left off when we get another call.
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();
  EXPECT_FALSE(dispatcher_.HasPendingWrites());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
