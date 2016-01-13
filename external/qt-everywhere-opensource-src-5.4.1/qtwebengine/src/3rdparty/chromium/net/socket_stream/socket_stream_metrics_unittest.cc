// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket_stream/socket_stream_metrics.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using base::Histogram;
using base::HistogramBase;
using base::HistogramSamples;
using base::StatisticsRecorder;

namespace net {

TEST(SocketStreamMetricsTest, ProtocolType) {
  // First we'll preserve the original values. We need to do this
  // as histograms can get affected by other tests. In particular,
  // SocketStreamTest and WebSocketTest can affect the histograms.
  scoped_ptr<HistogramSamples> original;
  HistogramBase* histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ProtocolType");
  if (histogram) {
    original = histogram->SnapshotSamples();
  }

  SocketStreamMetrics unknown(GURL("unknown://www.example.com/"));
  SocketStreamMetrics ws1(GURL("ws://www.example.com/"));
  SocketStreamMetrics ws2(GURL("ws://www.example.com/"));
  SocketStreamMetrics wss1(GURL("wss://www.example.com/"));
  SocketStreamMetrics wss2(GURL("wss://www.example.com/"));
  SocketStreamMetrics wss3(GURL("wss://www.example.com/"));

  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ProtocolType");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, histogram->flags());

  scoped_ptr<HistogramSamples> samples(histogram->SnapshotSamples());
  if (original.get()) {
    samples->Subtract(*original);  // Cancel the original values.
  }
  EXPECT_EQ(1, samples->GetCount(SocketStreamMetrics::PROTOCOL_UNKNOWN));
  EXPECT_EQ(2, samples->GetCount(SocketStreamMetrics::PROTOCOL_WEBSOCKET));
  EXPECT_EQ(3,
            samples->GetCount(SocketStreamMetrics::PROTOCOL_WEBSOCKET_SECURE));
}

TEST(SocketStreamMetricsTest, ConnectionType) {
  // First we'll preserve the original values.
  scoped_ptr<HistogramSamples> original;
  HistogramBase* histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ConnectionType");
  if (histogram) {
    original = histogram->SnapshotSamples();
  }

  SocketStreamMetrics metrics(GURL("ws://www.example.com/"));
  for (int i = 0; i < 1; ++i)
    metrics.OnStartConnection();
  for (int i = 0; i < 2; ++i)
    metrics.OnCountConnectionType(SocketStreamMetrics::TUNNEL_CONNECTION);
  for (int i = 0; i < 3; ++i)
    metrics.OnCountConnectionType(SocketStreamMetrics::SOCKS_CONNECTION);
  for (int i = 0; i < 4; ++i)
    metrics.OnCountConnectionType(SocketStreamMetrics::SSL_CONNECTION);


  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ConnectionType");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, histogram->flags());

  scoped_ptr<HistogramSamples> samples(histogram->SnapshotSamples());
  if (original.get()) {
    samples->Subtract(*original);  // Cancel the original values.
  }
  EXPECT_EQ(1, samples->GetCount(SocketStreamMetrics::ALL_CONNECTIONS));
  EXPECT_EQ(2, samples->GetCount(SocketStreamMetrics::TUNNEL_CONNECTION));
  EXPECT_EQ(3, samples->GetCount(SocketStreamMetrics::SOCKS_CONNECTION));
  EXPECT_EQ(4, samples->GetCount(SocketStreamMetrics::SSL_CONNECTION));
}

TEST(SocketStreamMetricsTest, WireProtocolType) {
  // First we'll preserve the original values.
  scoped_ptr<HistogramSamples> original;
  HistogramBase* histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.WireProtocolType");
  if (histogram) {
    original = histogram->SnapshotSamples();
  }

  SocketStreamMetrics metrics(GURL("ws://www.example.com/"));
  for (int i = 0; i < 3; ++i)
    metrics.OnCountWireProtocolType(
        SocketStreamMetrics::WIRE_PROTOCOL_WEBSOCKET);
  for (int i = 0; i < 7; ++i)
    metrics.OnCountWireProtocolType(SocketStreamMetrics::WIRE_PROTOCOL_SPDY);

  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.WireProtocolType");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, histogram->flags());

  scoped_ptr<HistogramSamples> samples(histogram->SnapshotSamples());
  if (original.get()) {
    samples->Subtract(*original);  // Cancel the original values.
  }
  EXPECT_EQ(3, samples->GetCount(SocketStreamMetrics::WIRE_PROTOCOL_WEBSOCKET));
  EXPECT_EQ(7, samples->GetCount(SocketStreamMetrics::WIRE_PROTOCOL_SPDY));
}

TEST(SocketStreamMetricsTest, OtherNumbers) {
  // First we'll preserve the original values.
  int64 original_received_bytes = 0;
  int64 original_received_counts = 0;
  int64 original_sent_bytes = 0;
  int64 original_sent_counts = 0;

  scoped_ptr<HistogramSamples> original;

  HistogramBase* histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ReceivedBytes");
  if (histogram) {
    original = histogram->SnapshotSamples();
    original_received_bytes = original->sum();
  }
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ReceivedCounts");
  if (histogram) {
    original = histogram->SnapshotSamples();
    original_received_counts = original->sum();
  }
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.SentBytes");
  if (histogram) {
    original = histogram->SnapshotSamples();
    original_sent_bytes = original->sum();
  }
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.SentCounts");
  if (histogram) {
    original = histogram->SnapshotSamples();
    original_sent_counts = original->sum();
  }

  SocketStreamMetrics metrics(GURL("ws://www.example.com/"));
  metrics.OnWaitConnection();
  metrics.OnStartConnection();
  metrics.OnConnected();
  metrics.OnRead(1);
  metrics.OnRead(10);
  metrics.OnWrite(2);
  metrics.OnWrite(20);
  metrics.OnWrite(200);
  metrics.OnClose();

  scoped_ptr<HistogramSamples> samples;

  // ConnectionLatency.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ConnectionLatency");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, histogram->flags());
  // We don't check the contents of the histogram as it's time sensitive.

  // ConnectionEstablish.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ConnectionEstablish");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, histogram->flags());
  // We don't check the contents of the histogram as it's time sensitive.

  // Duration.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.Duration");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, histogram->flags());
  // We don't check the contents of the histogram as it's time sensitive.

  // ReceivedBytes.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ReceivedBytes");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, histogram->flags());
  samples = histogram->SnapshotSamples();
  EXPECT_EQ(11, samples->sum() - original_received_bytes);  // 11 bytes read.

  // ReceivedCounts.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ReceivedCounts");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, histogram->flags());
  samples = histogram->SnapshotSamples();
  EXPECT_EQ(2, samples->sum() - original_received_counts);  // 2 read requests.

  // SentBytes.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.SentBytes");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, histogram->flags());
  samples = histogram->SnapshotSamples();
  EXPECT_EQ(222, samples->sum() - original_sent_bytes);  // 222 bytes sent.

  // SentCounts.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.SentCounts");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, histogram->flags());
  samples = histogram->SnapshotSamples();
  EXPECT_EQ(3, samples->sum() - original_sent_counts);  // 3 write requests.
}

}  // namespace net
