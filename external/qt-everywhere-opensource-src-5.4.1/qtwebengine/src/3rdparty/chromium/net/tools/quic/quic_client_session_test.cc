// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client_session.h"

#include <vector>

#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/quic_spdy_client_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::CryptoTestUtils;
using net::test::DefaultQuicConfig;
using net::test::PacketSavingConnection;
using net::test::SupportedVersions;
using testing::_;

namespace net {
namespace tools {
namespace test {
namespace {

const char kServerHostname[] = "www.example.com";
const uint16 kPort = 80;

class ToolsQuicClientSessionTest
    : public ::testing::TestWithParam<QuicVersion> {
 protected:
  ToolsQuicClientSessionTest()
      : connection_(new PacketSavingConnection(false,
                                               SupportedVersions(GetParam()))) {
    crypto_config_.SetDefaults();
    session_.reset(new QuicClientSession(
        QuicServerId(kServerHostname, kPort, false, PRIVACY_MODE_DISABLED),
        DefaultQuicConfig(),
        connection_,
        &crypto_config_));
    session_->config()->SetDefaults();
  }

  void CompleteCryptoHandshake() {
    ASSERT_TRUE(session_->CryptoConnect());
    CryptoTestUtils::HandshakeWithFakeServer(
        connection_, session_->GetCryptoStream());
  }

  PacketSavingConnection* connection_;
  scoped_ptr<QuicClientSession> session_;
  QuicCryptoClientConfig crypto_config_;
};

INSTANTIATE_TEST_CASE_P(Tests, ToolsQuicClientSessionTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(ToolsQuicClientSessionTest, CryptoConnect) {
  CompleteCryptoHandshake();
}

TEST_P(ToolsQuicClientSessionTest, MaxNumStreams) {
  session_->config()->set_max_streams_per_connection(1, 1);
  // FLAGS_max_streams_per_connection = 1;
  // Initialize crypto before the client session will create a stream.
  CompleteCryptoHandshake();

  QuicSpdyClientStream* stream =
      session_->CreateOutgoingDataStream();
  ASSERT_TRUE(stream);
  EXPECT_FALSE(session_->CreateOutgoingDataStream());

  // Close a stream and ensure I can now open a new one.
  session_->CloseStream(stream->id());
  stream = session_->CreateOutgoingDataStream();
  EXPECT_TRUE(stream);
}

TEST_P(ToolsQuicClientSessionTest, GoAwayReceived) {
  CompleteCryptoHandshake();

  // After receiving a GoAway, I should no longer be able to create outgoing
  // streams.
  session_->OnGoAway(QuicGoAwayFrame(QUIC_PEER_GOING_AWAY, 1u, "Going away."));
  EXPECT_EQ(NULL, session_->CreateOutgoingDataStream());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
