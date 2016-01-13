// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_client_stream.h"

#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

const char kServerHostname[] = "example.com";
const uint16 kServerPort = 80;

class QuicCryptoClientStreamTest : public ::testing::Test {
 public:
  QuicCryptoClientStreamTest()
      : connection_(new PacketSavingConnection(false)),
        session_(new TestClientSession(connection_, DefaultQuicConfig())),
        server_id_(kServerHostname, kServerPort, false, PRIVACY_MODE_DISABLED),
        stream_(new QuicCryptoClientStream(
            server_id_, session_.get(), NULL, &crypto_config_)) {
    session_->SetCryptoStream(stream_.get());
    session_->config()->SetDefaults();
    crypto_config_.SetDefaults();
  }

  void CompleteCryptoHandshake() {
    EXPECT_TRUE(stream_->CryptoConnect());
    CryptoTestUtils::HandshakeWithFakeServer(connection_, stream_.get());
  }

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_.reset(framer.ConstructHandshakeMessage(message_));
  }

  PacketSavingConnection* connection_;
  scoped_ptr<TestClientSession> session_;
  QuicServerId server_id_;
  scoped_ptr<QuicCryptoClientStream> stream_;
  CryptoHandshakeMessage message_;
  scoped_ptr<QuicData> message_data_;
  QuicCryptoClientConfig crypto_config_;
};

TEST_F(QuicCryptoClientStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream_->encryption_established());
  EXPECT_FALSE(stream_->handshake_confirmed());
}

TEST_F(QuicCryptoClientStreamTest, ConnectedAfterSHLO) {
  CompleteCryptoHandshake();
  EXPECT_TRUE(stream_->encryption_established());
  EXPECT_TRUE(stream_->handshake_confirmed());
}

TEST_F(QuicCryptoClientStreamTest, MessageAfterHandshake) {
  CompleteCryptoHandshake();

  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE));
  message_.set_tag(kCHLO);
  ConstructHandshakeMessage();
  stream_->ProcessRawData(message_data_->data(), message_data_->length());
}

TEST_F(QuicCryptoClientStreamTest, BadMessageType) {
  EXPECT_TRUE(stream_->CryptoConnect());

  message_.set_tag(kCHLO);
  ConstructHandshakeMessage();

  EXPECT_CALL(*connection_, SendConnectionCloseWithDetails(
        QUIC_INVALID_CRYPTO_MESSAGE_TYPE, "Expected REJ"));
  stream_->ProcessRawData(message_data_->data(), message_data_->length());
}

TEST_F(QuicCryptoClientStreamTest, NegotiatedParameters) {
  CompleteCryptoHandshake();

  const QuicConfig* config = session_->config();
  EXPECT_EQ(FLAGS_enable_quic_pacing ? kPACE : kQBIC,
            config->congestion_feedback());
  EXPECT_EQ(kDefaultTimeoutSecs,
            config->idle_connection_state_lifetime().ToSeconds());
  EXPECT_EQ(kDefaultMaxStreamsPerConnection,
            config->max_streams_per_connection());
  EXPECT_EQ(0, config->keepalive_timeout().ToSeconds());

  const QuicCryptoNegotiatedParameters& crypto_params(
      stream_->crypto_negotiated_params());
  EXPECT_EQ(crypto_config_.aead[0], crypto_params.aead);
  EXPECT_EQ(crypto_config_.kexs[0], crypto_params.key_exchange);
}

TEST_F(QuicCryptoClientStreamTest, InvalidHostname) {
  QuicServerId server_id("invalid", 80, false, PRIVACY_MODE_DISABLED);
  stream_.reset(new QuicCryptoClientStream(server_id, session_.get(), NULL,
                                           &crypto_config_));
  session_->SetCryptoStream(stream_.get());

  CompleteCryptoHandshake();
  EXPECT_TRUE(stream_->encryption_established());
  EXPECT_TRUE(stream_->handshake_confirmed());
}

TEST_F(QuicCryptoClientStreamTest, ExpiredServerConfig) {
  // Seed the config with a cached server config.
  CompleteCryptoHandshake();

  connection_ = new PacketSavingConnection(true);
  session_.reset(new TestClientSession(connection_, DefaultQuicConfig()));
  stream_.reset(new QuicCryptoClientStream(server_id_, session_.get(), NULL,
                                           &crypto_config_));

  session_->SetCryptoStream(stream_.get());
  session_->config()->SetDefaults();

  // Advance time 5 years to ensure that we pass the expiry time of the cached
  // server config.
  reinterpret_cast<MockClock*>(const_cast<QuicClock*>(connection_->clock()))
      ->AdvanceTime(QuicTime::Delta::FromSeconds(60 * 60 * 24 * 365 * 5));

  // Check that a client hello was sent and that CryptoConnect doesn't fail
  // with an error.
  EXPECT_TRUE(stream_->CryptoConnect());
  ASSERT_EQ(1u, connection_->packets_.size());
}

}  // namespace
}  // namespace test
}  // namespace net
