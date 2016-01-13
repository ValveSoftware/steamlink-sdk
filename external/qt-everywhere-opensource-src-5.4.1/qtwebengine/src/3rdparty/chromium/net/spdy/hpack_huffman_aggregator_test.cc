// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/spdy/hpack_huffman_aggregator.h"

#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Pair;

namespace {
const char kHistogramName[] = "Net.SpdyHpackEncodedCharacterFrequency";
}  // namespace

namespace test {

class HpackHuffmanAggregatorPeer {
 public:
  explicit HpackHuffmanAggregatorPeer(HpackHuffmanAggregator* agg)
      : agg_(agg) {}

  std::vector<size_t>* counts() {
    return &agg_->counts_;
  }
  HpackHuffmanAggregator::OriginEncoders* encoders() {
    return &agg_->encoders_;
  }
  size_t total_counts() {
    return agg_->total_counts_;
  }
  void set_total_counts(size_t total_counts) {
    agg_->total_counts_ = total_counts;
  }
  void set_max_encoders(size_t max_encoders) {
    agg_->max_encoders_ = max_encoders;
  }
  static bool IsCrossOrigin(const HttpRequestInfo& request) {
    return HpackHuffmanAggregator::IsCrossOrigin(request);
  }
  static void CreateSpdyHeadersFromHttpResponse(
      const HttpResponseHeaders& headers,
      SpdyHeaderBlock* headers_out) {
    HpackHuffmanAggregator::CreateSpdyHeadersFromHttpResponse(
        headers, headers_out);
  }
  HpackEncoder* ObtainEncoder(const SpdySessionKey& key) {
    return agg_->ObtainEncoder(key);
  }
  void PublishCounts() {
    agg_->PublishCounts();
  }

 private:
  HpackHuffmanAggregator* agg_;
};

}  // namespace test

class HpackHuffmanAggregatorTest : public ::testing::Test {
 protected:
  HpackHuffmanAggregatorTest()
      : peer_(&agg_) {}

  HpackHuffmanAggregator agg_;
  test::HpackHuffmanAggregatorPeer peer_;
};

TEST_F(HpackHuffmanAggregatorTest, CrossOriginDetermination) {
  HttpRequestInfo request;
  request.url = GURL("https://www.foo.com/a/page");

  // Main load without referer.
  request.load_flags = LOAD_MAIN_FRAME;
  EXPECT_FALSE(peer_.IsCrossOrigin(request));

  // Non-main load without referer. Treated as cross-origin.
  request.load_flags = 0;
  EXPECT_TRUE(peer_.IsCrossOrigin(request));

  // Main load with different referer origin.
  request.load_flags = LOAD_MAIN_FRAME;
  request.extra_headers.SetHeader(HttpRequestHeaders::kReferer,
                                  "https://www.bar.com/other/page");
  EXPECT_FALSE(peer_.IsCrossOrigin(request));

  // Non-main load with different referer orign.
  request.load_flags = 0;
  EXPECT_TRUE(peer_.IsCrossOrigin(request));

  // Non-main load with same referer orign.
  request.extra_headers.SetHeader(HttpRequestHeaders::kReferer,
                                  "https://www.foo.com/other/page");
  EXPECT_FALSE(peer_.IsCrossOrigin(request));

  // Non-main load with same referer host but different schemes.
  request.extra_headers.SetHeader(HttpRequestHeaders::kReferer,
                                  "http://www.foo.com/other/page");
  EXPECT_TRUE(peer_.IsCrossOrigin(request));
}

TEST_F(HpackHuffmanAggregatorTest, EncoderLRUQueue) {
  peer_.set_max_encoders(2);

  SpdySessionKey key1(HostPortPair("one.com", 443), ProxyServer::Direct(),
                      PRIVACY_MODE_ENABLED);
  SpdySessionKey key2(HostPortPair("two.com", 443), ProxyServer::Direct(),
                      PRIVACY_MODE_ENABLED);
  SpdySessionKey key3(HostPortPair("three.com", 443), ProxyServer::Direct(),
                      PRIVACY_MODE_ENABLED);

  // Creates one.com.
  HpackEncoder* one = peer_.ObtainEncoder(key1);
  EXPECT_EQ(1u, peer_.encoders()->size());

  // Creates two.com. No evictions.
  HpackEncoder* two = peer_.ObtainEncoder(key2);
  EXPECT_EQ(2u, peer_.encoders()->size());
  EXPECT_NE(one, two);

  // Touch one.com.
  EXPECT_EQ(one, peer_.ObtainEncoder(key1));

  // Creates three.com. Evicts two.com, as it's least-recently used.
  HpackEncoder* three = peer_.ObtainEncoder(key3);
  EXPECT_EQ(one, peer_.ObtainEncoder(key1));
  EXPECT_NE(one, three);
  EXPECT_EQ(2u, peer_.encoders()->size());
}

TEST_F(HpackHuffmanAggregatorTest, PublishCounts) {
  (*peer_.counts())[0] = 1;
  (*peer_.counts())[255] = 10;
  (*peer_.counts())[128] = 101;
  peer_.set_total_counts(112);

  peer_.PublishCounts();

  // Internal counts were reset after being published.
  EXPECT_THAT(*peer_.counts(), Each(Eq(0u)));
  EXPECT_EQ(0u, peer_.total_counts());

  // Verify histogram counts match the expectation.
  scoped_ptr<base::HistogramSamples> samples =
      base::StatisticsRecorder::FindHistogram(kHistogramName)
      ->SnapshotSamples();

  EXPECT_EQ(0, samples->GetCount(0));
  EXPECT_EQ(1, samples->GetCount(1));
  EXPECT_EQ(101, samples->GetCount(129));
  EXPECT_EQ(10, samples->GetCount(256));
  EXPECT_EQ(112, samples->TotalCount());

  // Publish a second round of counts;
  (*peer_.counts())[1] = 32;
  (*peer_.counts())[128] = 5;
  peer_.set_total_counts(37);

  peer_.PublishCounts();

  // Verify they've been aggregated into the previous counts.
  samples = base::StatisticsRecorder::FindHistogram(kHistogramName)
      ->SnapshotSamples();

  EXPECT_EQ(0, samples->GetCount(0));
  EXPECT_EQ(1, samples->GetCount(1));
  EXPECT_EQ(32, samples->GetCount(2));
  EXPECT_EQ(106, samples->GetCount(129));
  EXPECT_EQ(10, samples->GetCount(256));
  EXPECT_EQ(149, samples->TotalCount());
}

TEST_F(HpackHuffmanAggregatorTest, CreateSpdyResponseHeaders) {
  char kRawHeaders[] =
    "HTTP/1.1    202   Accepted  \0"
    "Content-TYPE  : text/html; charset=utf-8  \0"
    "Set-Cookie: foo=bar \0"
    "Set-Cookie:   baz=bing \0"
    "Cache-Control: pragma=no-cache \0"
    "Cache-CONTROL: expires=12345 \0\0";

  scoped_refptr<HttpResponseHeaders> parsed_headers(new HttpResponseHeaders(
      std::string(kRawHeaders, arraysize(kRawHeaders) - 1)));

  SpdyHeaderBlock headers;
  peer_.CreateSpdyHeadersFromHttpResponse(*parsed_headers, &headers);
  EXPECT_THAT(headers, ElementsAre(
      Pair(":status", "202"),
      Pair("cache-control", std::string("pragma=no-cache\0expires=12345", 29)),
      Pair("content-type", "text/html; charset=utf-8"),
      Pair("set-cookie", std::string("foo=bar\0baz=bing", 16))));
}

}  // namespace net
