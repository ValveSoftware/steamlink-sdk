// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_config.h"

#include "net/quic/crypto/crypto_handshake_message.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_sent_packet_manager.h"
#include "net/quic/quic_time.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {
namespace test {
namespace {

class QuicConfigTest : public ::testing::Test {
 protected:
  QuicConfigTest() {
    config_.SetDefaults();
  }

  QuicConfig config_;
};

TEST_F(QuicConfigTest, ToHandshakeMessage) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_pacing, false);
  config_.SetDefaults();
  config_.SetInitialFlowControlWindowToSend(
      kInitialSessionFlowControlWindowForTest);
  config_.SetInitialStreamFlowControlWindowToSend(
      kInitialStreamFlowControlWindowForTest);
  config_.SetInitialSessionFlowControlWindowToSend(
      kInitialSessionFlowControlWindowForTest);
  config_.set_idle_connection_state_lifetime(QuicTime::Delta::FromSeconds(5),
                                             QuicTime::Delta::FromSeconds(2));
  config_.set_max_streams_per_connection(4, 2);
  CryptoHandshakeMessage msg;
  config_.ToHandshakeMessage(&msg);

  uint32 value;
  QuicErrorCode error = msg.GetUint32(kICSL, &value);
  EXPECT_EQ(QUIC_NO_ERROR, error);
  EXPECT_EQ(5u, value);

  error = msg.GetUint32(kMSPC, &value);
  EXPECT_EQ(QUIC_NO_ERROR, error);
  EXPECT_EQ(4u, value);

  error = msg.GetUint32(kIFCW, &value);
  EXPECT_EQ(QUIC_NO_ERROR, error);
  EXPECT_EQ(kInitialSessionFlowControlWindowForTest, value);

  error = msg.GetUint32(kSFCW, &value);
  EXPECT_EQ(QUIC_NO_ERROR, error);
  EXPECT_EQ(kInitialStreamFlowControlWindowForTest, value);

  error = msg.GetUint32(kCFCW, &value);
  EXPECT_EQ(QUIC_NO_ERROR, error);
  EXPECT_EQ(kInitialSessionFlowControlWindowForTest, value);

  const QuicTag* out;
  size_t out_len;
  error = msg.GetTaglist(kCGST, &out, &out_len);
  EXPECT_EQ(1u, out_len);
  EXPECT_EQ(kQBIC, *out);
}

TEST_F(QuicConfigTest, ToHandshakeMessageWithPacing) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_pacing, true);

  config_.SetDefaults();
  CryptoHandshakeMessage msg;
  config_.ToHandshakeMessage(&msg);

  const QuicTag* out;
  size_t out_len;
  EXPECT_EQ(QUIC_NO_ERROR, msg.GetTaglist(kCGST, &out, &out_len));
  EXPECT_EQ(2u, out_len);
  EXPECT_EQ(kPACE, out[0]);
  EXPECT_EQ(kQBIC, out[1]);
}

TEST_F(QuicConfigTest, ProcessClientHello) {
  QuicConfig client_config;
  QuicTagVector cgst;
  cgst.push_back(kINAR);
  cgst.push_back(kQBIC);
  client_config.set_congestion_feedback(cgst, kQBIC);
  client_config.set_idle_connection_state_lifetime(
      QuicTime::Delta::FromSeconds(2 * kDefaultTimeoutSecs),
      QuicTime::Delta::FromSeconds(kDefaultTimeoutSecs));
  client_config.set_max_streams_per_connection(
      2 * kDefaultMaxStreamsPerConnection, kDefaultMaxStreamsPerConnection);
  client_config.SetInitialRoundTripTimeUsToSend(
      10 * base::Time::kMicrosecondsPerMillisecond);
  client_config.SetInitialFlowControlWindowToSend(
      2 * kInitialSessionFlowControlWindowForTest);
  client_config.SetInitialStreamFlowControlWindowToSend(
      2 * kInitialStreamFlowControlWindowForTest);
  client_config.SetInitialSessionFlowControlWindowToSend(
      2 * kInitialSessionFlowControlWindowForTest);
  QuicTagVector copt;
  copt.push_back(kTBBR);
  client_config.SetCongestionOptionsToSend(copt);
  CryptoHandshakeMessage msg;
  client_config.ToHandshakeMessage(&msg);
  string error_details;
  const QuicErrorCode error =
      config_.ProcessPeerHello(msg, CLIENT, &error_details);
  EXPECT_EQ(QUIC_NO_ERROR, error);
  EXPECT_TRUE(config_.negotiated());
  EXPECT_EQ(kQBIC, config_.congestion_feedback());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kDefaultTimeoutSecs),
            config_.idle_connection_state_lifetime());
  EXPECT_EQ(kDefaultMaxStreamsPerConnection,
            config_.max_streams_per_connection());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(0), config_.keepalive_timeout());
  EXPECT_EQ(10 * base::Time::kMicrosecondsPerMillisecond,
            config_.ReceivedInitialRoundTripTimeUs());
  EXPECT_FALSE(config_.HasReceivedLossDetection());
  EXPECT_TRUE(config_.HasReceivedCongestionOptions());
  EXPECT_EQ(1u, config_.ReceivedCongestionOptions().size());
  EXPECT_EQ(config_.ReceivedCongestionOptions()[0], kTBBR);
  EXPECT_EQ(config_.ReceivedInitialFlowControlWindowBytes(),
            2 * kInitialSessionFlowControlWindowForTest);
  EXPECT_EQ(config_.ReceivedInitialStreamFlowControlWindowBytes(),
            2 * kInitialStreamFlowControlWindowForTest);
  EXPECT_EQ(config_.ReceivedInitialSessionFlowControlWindowBytes(),
            2 * kInitialSessionFlowControlWindowForTest);
}

TEST_F(QuicConfigTest, ProcessServerHello) {
  QuicConfig server_config;
  QuicTagVector cgst;
  cgst.push_back(kQBIC);
  server_config.set_congestion_feedback(cgst, kQBIC);
  server_config.set_idle_connection_state_lifetime(
      QuicTime::Delta::FromSeconds(kDefaultTimeoutSecs / 2),
      QuicTime::Delta::FromSeconds(kDefaultTimeoutSecs / 2));
  server_config.set_max_streams_per_connection(
      kDefaultMaxStreamsPerConnection / 2,
      kDefaultMaxStreamsPerConnection / 2);
  server_config.SetInitialCongestionWindowToSend(kDefaultInitialWindow / 2);
  server_config.SetInitialRoundTripTimeUsToSend(
      10 * base::Time::kMicrosecondsPerMillisecond);
  server_config.SetInitialFlowControlWindowToSend(
      2 * kInitialSessionFlowControlWindowForTest);
  server_config.SetInitialStreamFlowControlWindowToSend(
      2 * kInitialStreamFlowControlWindowForTest);
  server_config.SetInitialSessionFlowControlWindowToSend(
      2 * kInitialSessionFlowControlWindowForTest);
  CryptoHandshakeMessage msg;
  server_config.ToHandshakeMessage(&msg);
  string error_details;
  const QuicErrorCode error =
      config_.ProcessPeerHello(msg, SERVER, &error_details);
  EXPECT_EQ(QUIC_NO_ERROR, error);
  EXPECT_TRUE(config_.negotiated());
  EXPECT_EQ(kQBIC, config_.congestion_feedback());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kDefaultTimeoutSecs / 2),
            config_.idle_connection_state_lifetime());
  EXPECT_EQ(kDefaultMaxStreamsPerConnection / 2,
            config_.max_streams_per_connection());
  EXPECT_EQ(kDefaultInitialWindow / 2,
            config_.ReceivedInitialCongestionWindow());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(0), config_.keepalive_timeout());
  EXPECT_EQ(10 * base::Time::kMicrosecondsPerMillisecond,
            config_.ReceivedInitialRoundTripTimeUs());
  EXPECT_FALSE(config_.HasReceivedLossDetection());
  EXPECT_EQ(config_.ReceivedInitialFlowControlWindowBytes(),
            2 * kInitialSessionFlowControlWindowForTest);
  EXPECT_EQ(config_.ReceivedInitialStreamFlowControlWindowBytes(),
            2 * kInitialStreamFlowControlWindowForTest);
  EXPECT_EQ(config_.ReceivedInitialSessionFlowControlWindowBytes(),
            2 * kInitialSessionFlowControlWindowForTest);
}

TEST_F(QuicConfigTest, MissingOptionalValuesInCHLO) {
  CryptoHandshakeMessage msg;
  msg.SetValue(kICSL, 1);
  msg.SetVector(kCGST, QuicTagVector(1, kQBIC));

  // Set all REQUIRED tags.
  msg.SetValue(kICSL, 1);
  msg.SetVector(kCGST, QuicTagVector(1, kQBIC));
  msg.SetValue(kMSPC, 1);

  // No error, as rest are optional.
  string error_details;
  const QuicErrorCode error =
      config_.ProcessPeerHello(msg, CLIENT, &error_details);
  EXPECT_EQ(QUIC_NO_ERROR, error);

  EXPECT_FALSE(config_.HasReceivedInitialFlowControlWindowBytes());
}

TEST_F(QuicConfigTest, MissingOptionalValuesInSHLO) {
  CryptoHandshakeMessage msg;

  // Set all REQUIRED tags.
  msg.SetValue(kICSL, 1);
  msg.SetVector(kCGST, QuicTagVector(1, kQBIC));
  msg.SetValue(kMSPC, 1);

  // No error, as rest are optional.
  string error_details;
  const QuicErrorCode error =
      config_.ProcessPeerHello(msg, SERVER, &error_details);
  EXPECT_EQ(QUIC_NO_ERROR, error);

  EXPECT_FALSE(config_.HasReceivedInitialFlowControlWindowBytes());
}

TEST_F(QuicConfigTest, MissingValueInCHLO) {
  CryptoHandshakeMessage msg;
  msg.SetValue(kICSL, 1);
  msg.SetVector(kCGST, QuicTagVector(1, kQBIC));
  // Missing kMSPC. KATO is optional.
  string error_details;
  const QuicErrorCode error =
      config_.ProcessPeerHello(msg, CLIENT, &error_details);
  EXPECT_EQ(QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND, error);
}

TEST_F(QuicConfigTest, MissingValueInSHLO) {
  CryptoHandshakeMessage msg;
  msg.SetValue(kICSL, 1);
  msg.SetValue(kMSPC, 3);
  // Missing CGST. KATO is optional.
  string error_details;
  const QuicErrorCode error =
      config_.ProcessPeerHello(msg, SERVER, &error_details);
  EXPECT_EQ(QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND, error);
}

TEST_F(QuicConfigTest, OutOfBoundSHLO) {
  QuicConfig server_config;
  server_config.set_idle_connection_state_lifetime(
      QuicTime::Delta::FromSeconds(2 * kDefaultTimeoutSecs),
      QuicTime::Delta::FromSeconds(2 * kDefaultTimeoutSecs));

  CryptoHandshakeMessage msg;
  server_config.ToHandshakeMessage(&msg);
  string error_details;
  const QuicErrorCode error =
      config_.ProcessPeerHello(msg, SERVER, &error_details);
  EXPECT_EQ(QUIC_INVALID_NEGOTIATED_VALUE, error);
}

TEST_F(QuicConfigTest, MultipleNegotiatedValuesInVectorTag) {
  QuicConfig server_config;
  QuicTagVector cgst;
  cgst.push_back(kQBIC);
  cgst.push_back(kINAR);
  server_config.set_congestion_feedback(cgst, kQBIC);

  CryptoHandshakeMessage msg;
  server_config.ToHandshakeMessage(&msg);
  string error_details;
  const QuicErrorCode error =
      config_.ProcessPeerHello(msg, SERVER, &error_details);
  EXPECT_EQ(QUIC_INVALID_NEGOTIATED_VALUE, error);
}

TEST_F(QuicConfigTest, NoOverLapInCGST) {
  QuicConfig server_config;
  server_config.SetDefaults();
  QuicTagVector cgst;
  cgst.push_back(kINAR);
  server_config.set_congestion_feedback(cgst, kINAR);

  CryptoHandshakeMessage msg;
  string error_details;
  server_config.ToHandshakeMessage(&msg);
  const QuicErrorCode error =
      config_.ProcessPeerHello(msg, CLIENT, &error_details);
  DVLOG(1) << QuicUtils::ErrorToString(error);
  EXPECT_EQ(QUIC_CRYPTO_MESSAGE_PARAMETER_NO_OVERLAP, error);
}

TEST_F(QuicConfigTest, InvalidFlowControlWindow) {
  // QuicConfig should not accept an invalid flow control window to send to the
  // peer: the receive window must be at least the default of 16 Kb.
  QuicConfig config;
  const uint64 kInvalidWindow = kDefaultFlowControlSendWindow - 1;
  EXPECT_DFATAL(config.SetInitialFlowControlWindowToSend(kInvalidWindow),
                "Initial flow control receive window");

  EXPECT_EQ(kDefaultFlowControlSendWindow,
            config.GetInitialFlowControlWindowToSend());
}

}  // namespace
}  // namespace test
}  // namespace net
