// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_logger.h"

#include "net/cert/x509_certificate.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class QuicConnectionLoggerPeer {
 public:
  static size_t num_truncated_acks_sent(const QuicConnectionLogger& logger) {
    return logger.num_truncated_acks_sent_;
  }

  static void set_num_packets_received(QuicConnectionLogger* logger,
                                       int value) {
    logger->num_packets_received_ = value;
  }

  static void set_largest_received_packet_number(QuicConnectionLogger* logger,
                                                 int value) {
    logger->largest_received_packet_number_ = value;
  }
};

class QuicConnectionLoggerTest : public ::testing::Test {
 protected:
  QuicConnectionLoggerTest()
      : session_(new MockQuicConnection(&helper_,
                                        &alarm_factory_,
                                        Perspective::IS_CLIENT)),
        logger_(&session_,
                "CONNECTION_UNKNOWN",
                /*socket_performance_watcher=*/nullptr,
                net_log_) {
    QuicConnectionPeer::GetFramer(session_.connection())
        ->set_version(QUIC_VERSION_33);
  }

  BoundNetLog net_log_;
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicSpdySession session_;
  QuicConnectionLogger logger_;
};

TEST_F(QuicConnectionLoggerTest, TruncatedAcksSentNotChanged) {
  QuicAckFrame frame;
  logger_.OnFrameAddedToPacket(QuicFrame(&frame));
  EXPECT_EQ(0u, QuicConnectionLoggerPeer::num_truncated_acks_sent(logger_));

  for (QuicPacketNumber i = 0; i < 256; ++i) {
    frame.packets.Add(i);
  }
  logger_.OnFrameAddedToPacket(QuicFrame(&frame));
  EXPECT_EQ(0u, QuicConnectionLoggerPeer::num_truncated_acks_sent(logger_));
}

TEST_F(QuicConnectionLoggerTest, TruncatedAcksSent) {
  QuicAckFrame frame;
  for (QuicPacketNumber i = 0; i < 512; i += 2) {
    frame.packets.Add(i);
  }
  logger_.OnFrameAddedToPacket(QuicFrame(&frame));
  EXPECT_EQ(1u, QuicConnectionLoggerPeer::num_truncated_acks_sent(logger_));
}

TEST_F(QuicConnectionLoggerTest, ReceivedPacketLossRate) {
  QuicConnectionLoggerPeer::set_num_packets_received(&logger_, 1);
  QuicConnectionLoggerPeer::set_largest_received_packet_number(&logger_, 2);
  EXPECT_EQ(0.5f, logger_.ReceivedPacketLossRate());
}

}  // namespace test
}  // namespace net
