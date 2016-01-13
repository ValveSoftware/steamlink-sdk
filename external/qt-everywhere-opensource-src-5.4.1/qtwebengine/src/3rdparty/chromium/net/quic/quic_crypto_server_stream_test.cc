// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_server_stream.h"

#include <map>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/delayed_verify_strike_register_client.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
class QuicConnection;
class ReliableQuicStream;
}  // namespace net

using std::pair;
using testing::_;

namespace net {
namespace test {

class QuicCryptoServerConfigPeer {
 public:
  static string GetPrimaryOrbit(const QuicCryptoServerConfig& config) {
    base::AutoLock lock(config.configs_lock_);
    CHECK(config.primary_config_ != NULL);
    return string(reinterpret_cast<const char*>(config.primary_config_->orbit),
                  kOrbitSize);
  }
};

namespace {

const char kServerHostname[] = "test.example.com";
const uint16 kServerPort = 80;

class QuicCryptoServerStreamTest : public ::testing::TestWithParam<bool> {
 public:
  QuicCryptoServerStreamTest()
      : connection_(new PacketSavingConnection(true)),
        session_(connection_, DefaultQuicConfig()),
        crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance()),
        stream_(crypto_config_, &session_),
        strike_register_client_(NULL) {
    config_.SetDefaults();
    session_.config()->SetDefaults();
    session_.SetCryptoStream(&stream_);
    // We advance the clock initially because the default time is zero and the
    // strike register worries that we've just overflowed a uint32 time.
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(100000));
    // TODO(rtenneti): Enable testing of ProofSource.
    // crypto_config_.SetProofSource(CryptoTestUtils::ProofSourceForTesting());
    crypto_config_.set_strike_register_no_startup_period();

    CryptoTestUtils::SetupCryptoServerConfigForTest(
        connection_->clock(), connection_->random_generator(),
        session_.config(), &crypto_config_);

    if (AsyncStrikeRegisterVerification()) {
      string orbit =
          QuicCryptoServerConfigPeer::GetPrimaryOrbit(crypto_config_);
      strike_register_client_ = new DelayedVerifyStrikeRegisterClient(
          10000,  // strike_register_max_entries
          static_cast<uint32>(connection_->clock()->WallNow().ToUNIXSeconds()),
          60,  // strike_register_window_secs
          reinterpret_cast<const uint8 *>(orbit.data()),
          StrikeRegister::NO_STARTUP_PERIOD_NEEDED);
      strike_register_client_->StartDelayingVerification();
      crypto_config_.SetStrikeRegisterClient(strike_register_client_);
    }
  }

  bool AsyncStrikeRegisterVerification() {
    return GetParam();
  }

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_.reset(framer.ConstructHandshakeMessage(message_));
  }

  int CompleteCryptoHandshake() {
    return CryptoTestUtils::HandshakeWithFakeClient(connection_, &stream_,
                                                    client_options_);
  }

 protected:
  PacketSavingConnection* connection_;
  TestClientSession session_;
  QuicConfig config_;
  QuicCryptoServerConfig crypto_config_;
  QuicCryptoServerStream stream_;
  CryptoHandshakeMessage message_;
  scoped_ptr<QuicData> message_data_;
  CryptoTestUtils::FakeClientOptions client_options_;
  DelayedVerifyStrikeRegisterClient* strike_register_client_;
};

INSTANTIATE_TEST_CASE_P(Tests, QuicCryptoServerStreamTest, testing::Bool());

TEST_P(QuicCryptoServerStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream_.encryption_established());
  EXPECT_FALSE(stream_.handshake_confirmed());
}

TEST_P(QuicCryptoServerStreamTest, ConnectedAfterCHLO) {
  // CompleteCryptoHandshake returns the number of client hellos sent. This
  // test should send:
  //   * One to get a source-address token and certificates.
  //   * One to complete the handshake.
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(stream_.encryption_established());
  EXPECT_TRUE(stream_.handshake_confirmed());
}

TEST_P(QuicCryptoServerStreamTest, ZeroRTT) {
  PacketSavingConnection* client_conn = new PacketSavingConnection(false);
  PacketSavingConnection* server_conn = new PacketSavingConnection(false);
  client_conn->AdvanceTime(QuicTime::Delta::FromSeconds(100000));
  server_conn->AdvanceTime(QuicTime::Delta::FromSeconds(100000));

  QuicConfig client_config;
  client_config.SetDefaults();
  scoped_ptr<TestClientSession> client_session(
      new TestClientSession(client_conn, client_config));
  QuicCryptoClientConfig client_crypto_config;
  client_crypto_config.SetDefaults();

  QuicServerId server_id(kServerHostname, kServerPort, false,
                         PRIVACY_MODE_DISABLED);
  scoped_ptr<QuicCryptoClientStream> client(new QuicCryptoClientStream(
      server_id, client_session.get(), NULL, &client_crypto_config));
  client_session->SetCryptoStream(client.get());

  // Do a first handshake in order to prime the client config with the server's
  // information.
  CHECK(client->CryptoConnect());
  CHECK_EQ(1u, client_conn->packets_.size());

  scoped_ptr<TestSession> server_session(new TestSession(server_conn, config_));
  scoped_ptr<QuicCryptoServerStream> server(
      new QuicCryptoServerStream(crypto_config_, server_session.get()));
  server_session->SetCryptoStream(server.get());

  CryptoTestUtils::CommunicateHandshakeMessages(
      client_conn, client.get(), server_conn, server.get());
  EXPECT_EQ(2, client->num_sent_client_hellos());

  // Now do another handshake, hopefully in 0-RTT.
  LOG(INFO) << "Resetting for 0-RTT handshake attempt";

  client_conn = new PacketSavingConnection(false);
  server_conn = new PacketSavingConnection(false);
  // We need to advance time past the strike-server window so that it's
  // authoritative in this time span.
  client_conn->AdvanceTime(QuicTime::Delta::FromSeconds(102000));
  server_conn->AdvanceTime(QuicTime::Delta::FromSeconds(102000));

  // This causes the client's nonce to be different and thus stops the
  // strike-register from rejecting the repeated nonce.
  reinterpret_cast<MockRandom*>(client_conn->random_generator())->ChangeValue();
  client_session.reset(new TestClientSession(client_conn, client_config));
  server_session.reset(new TestSession(server_conn, config_));
  client.reset(new QuicCryptoClientStream(
      server_id, client_session.get(), NULL, &client_crypto_config));
  client_session->SetCryptoStream(client.get());

  server.reset(new QuicCryptoServerStream(crypto_config_,
                                          server_session.get()));
  server_session->SetCryptoStream(server.get());

  CHECK(client->CryptoConnect());

  if (AsyncStrikeRegisterVerification()) {
    EXPECT_FALSE(client->handshake_confirmed());
    EXPECT_FALSE(server->handshake_confirmed());

    // Advance the handshake.  Expect that the server will be stuck
    // waiting for client nonce verification to complete.
    pair<size_t, size_t> messages_moved = CryptoTestUtils::AdvanceHandshake(
        client_conn, client.get(), 0, server_conn, server.get(), 0);
    EXPECT_EQ(1u, messages_moved.first);
    EXPECT_EQ(0u, messages_moved.second);
    EXPECT_EQ(1, strike_register_client_->PendingVerifications());
    EXPECT_FALSE(client->handshake_confirmed());
    EXPECT_FALSE(server->handshake_confirmed());

    // The server handshake completes once the nonce verification completes.
    strike_register_client_->RunPendingVerifications();
    EXPECT_FALSE(client->handshake_confirmed());
    EXPECT_TRUE(server->handshake_confirmed());

    messages_moved = CryptoTestUtils::AdvanceHandshake(
        client_conn, client.get(), messages_moved.first,
        server_conn, server.get(), messages_moved.second);
    EXPECT_EQ(1u, messages_moved.first);
    EXPECT_EQ(1u, messages_moved.second);
    EXPECT_TRUE(client->handshake_confirmed());
    EXPECT_TRUE(server->handshake_confirmed());
  } else {
    CryptoTestUtils::CommunicateHandshakeMessages(
        client_conn, client.get(), server_conn, server.get());
  }

  EXPECT_EQ(1, client->num_sent_client_hellos());
}

TEST_P(QuicCryptoServerStreamTest, MessageAfterHandshake) {
  CompleteCryptoHandshake();
  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE));
  message_.set_tag(kCHLO);
  ConstructHandshakeMessage();
  stream_.ProcessRawData(message_data_->data(), message_data_->length());
}

TEST_P(QuicCryptoServerStreamTest, BadMessageType) {
  message_.set_tag(kSHLO);
  ConstructHandshakeMessage();
  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_INVALID_CRYPTO_MESSAGE_TYPE));
  stream_.ProcessRawData(message_data_->data(), message_data_->length());
}

TEST_P(QuicCryptoServerStreamTest, WithoutCertificates) {
  crypto_config_.SetProofSource(NULL);
  client_options_.dont_verify_certs = true;

  // Only 2 client hellos need to be sent in the no-certs case: one to get the
  // source-address token and the second to finish.
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(stream_.encryption_established());
  EXPECT_TRUE(stream_.handshake_confirmed());
}

TEST_P(QuicCryptoServerStreamTest, ChannelID) {
  client_options_.channel_id_enabled = true;
  // CompleteCryptoHandshake verifies
  // stream_.crypto_negotiated_params().channel_id is correct.
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(stream_.encryption_established());
  EXPECT_TRUE(stream_.handshake_confirmed());
}

}  // namespace
}  // namespace test
}  // namespace net
