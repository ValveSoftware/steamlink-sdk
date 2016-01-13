// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_uma_histograms.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace content {

class MockPerSessionWebRTCAPIMetrics : public PerSessionWebRTCAPIMetrics {
 public:
  MockPerSessionWebRTCAPIMetrics() {}

  using PerSessionWebRTCAPIMetrics::LogUsageOnlyOnce;

  MOCK_METHOD1(LogUsage, void(JavaScriptAPIName));
};

TEST(PerSessionWebRTCAPIMetrics, NoCallOngoingGetUserMedia) {
  MockPerSessionWebRTCAPIMetrics metrics;
  EXPECT_CALL(metrics, LogUsage(_)).Times(1);
  metrics.LogUsageOnlyOnce(WEBKIT_GET_USER_MEDIA);
}

TEST(PerSessionWebRTCAPIMetrics, CallOngoingGetUserMedia) {
  MockPerSessionWebRTCAPIMetrics metrics;
  metrics.IncrementStreamCounter();
  EXPECT_CALL(metrics, LogUsage(WEBKIT_GET_USER_MEDIA)).Times(1);
  metrics.LogUsageOnlyOnce(WEBKIT_GET_USER_MEDIA);
}

TEST(PerSessionWebRTCAPIMetrics, NoCallOngoingGetMediaDevices) {
  MockPerSessionWebRTCAPIMetrics metrics;
  EXPECT_CALL(metrics, LogUsage(_)).Times(1);
  metrics.LogUsageOnlyOnce(WEBKIT_GET_MEDIA_DEVICES);
}

TEST(PerSessionWebRTCAPIMetrics, CallOngoingGetMediaDevices) {
  MockPerSessionWebRTCAPIMetrics metrics;
  metrics.IncrementStreamCounter();
  EXPECT_CALL(metrics, LogUsage(WEBKIT_GET_MEDIA_DEVICES)).Times(1);
  metrics.LogUsageOnlyOnce(WEBKIT_GET_MEDIA_DEVICES);
}

TEST(PerSessionWebRTCAPIMetrics, NoCallOngoingRTCPeerConnection) {
  MockPerSessionWebRTCAPIMetrics metrics;
  EXPECT_CALL(metrics, LogUsage(WEBKIT_RTC_PEER_CONNECTION));
  metrics.LogUsageOnlyOnce(WEBKIT_RTC_PEER_CONNECTION);
}

TEST(PerSessionWebRTCAPIMetrics, NoCallOngoingMultiplePC) {
  MockPerSessionWebRTCAPIMetrics metrics;
  EXPECT_CALL(metrics, LogUsage(WEBKIT_RTC_PEER_CONNECTION)).Times(1);
  metrics.LogUsageOnlyOnce(WEBKIT_RTC_PEER_CONNECTION);
  metrics.LogUsageOnlyOnce(WEBKIT_RTC_PEER_CONNECTION);
  metrics.LogUsageOnlyOnce(WEBKIT_RTC_PEER_CONNECTION);
}

TEST(PerSessionWebRTCAPIMetrics, BeforeAfterCallMultiplePC) {
  MockPerSessionWebRTCAPIMetrics metrics;
  EXPECT_CALL(metrics, LogUsage(WEBKIT_RTC_PEER_CONNECTION)).Times(1);
  metrics.LogUsageOnlyOnce(WEBKIT_RTC_PEER_CONNECTION);
  metrics.LogUsageOnlyOnce(WEBKIT_RTC_PEER_CONNECTION);
  metrics.IncrementStreamCounter();
  metrics.IncrementStreamCounter();
  metrics.LogUsageOnlyOnce(WEBKIT_RTC_PEER_CONNECTION);
  metrics.DecrementStreamCounter();
  metrics.LogUsageOnlyOnce(WEBKIT_RTC_PEER_CONNECTION);
  metrics.DecrementStreamCounter();
  EXPECT_CALL(metrics, LogUsage(WEBKIT_RTC_PEER_CONNECTION)).Times(1);
  metrics.LogUsageOnlyOnce(WEBKIT_RTC_PEER_CONNECTION);
  metrics.LogUsageOnlyOnce(WEBKIT_RTC_PEER_CONNECTION);
}

}  // namespace content
