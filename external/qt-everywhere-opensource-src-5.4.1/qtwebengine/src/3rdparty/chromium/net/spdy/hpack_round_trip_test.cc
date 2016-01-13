// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <ctime>
#include <map>
#include <string>
#include <vector>

#include "base/rand_util.h"
#include "net/spdy/hpack_constants.h"
#include "net/spdy/hpack_decoder.h"
#include "net/spdy/hpack_encoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using std::map;
using std::string;
using std::vector;

namespace {

class HpackRoundTripTest : public ::testing::Test {
 protected:
  HpackRoundTripTest()
      : encoder_(ObtainHpackHuffmanTable()),
        decoder_(ObtainHpackHuffmanTable()) {}

  virtual void SetUp() {
    // Use a small table size to tickle eviction handling.
    encoder_.ApplyHeaderTableSizeSetting(256);
    decoder_.ApplyHeaderTableSizeSetting(256);
  }

  bool RoundTrip(const map<string, string>& header_set) {
    string encoded;
    encoder_.EncodeHeaderSet(header_set, &encoded);

    bool success = decoder_.HandleControlFrameHeadersData(
        1, encoded.data(), encoded.size());
    success &= decoder_.HandleControlFrameHeadersComplete(1);

    EXPECT_EQ(header_set, decoder_.decoded_block());
    return success;
  }

  size_t SampleExponential(size_t mean, size_t sanity_bound) {
    return std::min<size_t>(-std::log(base::RandDouble()) * mean,
                            sanity_bound);
  }

  HpackEncoder encoder_;
  HpackDecoder decoder_;
};

TEST_F(HpackRoundTripTest, ResponseFixtures) {
  {
    map<string, string> headers;
    headers[":status"] = "302";
    headers["cache-control"] = "private";
    headers["date"] = "Mon, 21 Oct 2013 20:13:21 GMT";
    headers["location"] = "https://www.example.com";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    map<string, string> headers;
    headers[":status"] = "200";
    headers["cache-control"] = "private";
    headers["date"] = "Mon, 21 Oct 2013 20:13:21 GMT";
    headers["location"] = "https://www.example.com";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    map<string, string> headers;
    headers[":status"] = "200";
    headers["cache-control"] = "private";
    headers["content-encoding"] = "gzip";
    headers["date"] = "Mon, 21 Oct 2013 20:13:22 GMT";
    headers["location"] = "https://www.example.com";
    headers["set-cookie"] = "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU;"
        " max-age=3600; version=1";
    EXPECT_TRUE(RoundTrip(headers));
  }
}

TEST_F(HpackRoundTripTest, RequestFixtures) {
  {
    map<string, string> headers;
    headers[":authority"] = "www.example.com";
    headers[":method"] = "GET";
    headers[":path"] = "/";
    headers[":scheme"] = "http";
    headers["cookie"] = "baz=bing; foo=bar";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    map<string, string> headers;
    headers[":authority"] = "www.example.com";
    headers[":method"] = "GET";
    headers[":path"] = "/";
    headers[":scheme"] = "http";
    headers["cache-control"] = "no-cache";
    headers["cookie"] = "fizzle=fazzle; foo=bar";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    map<string, string> headers;
    headers[":authority"] = "www.example.com";
    headers[":method"] = "GET";
    headers[":path"] = "/index.html";
    headers[":scheme"] = "https";
    headers["custom-key"] = "custom-value";
    headers["cookie"] = "baz=bing; fizzle=fazzle; garbage";
    EXPECT_TRUE(RoundTrip(headers));
  }
}

TEST_F(HpackRoundTripTest, RandomizedExamples) {
  // Grow vectors of names & values, which are seeded with fixtures and then
  // expanded with dynamically generated data. Samples are taken using the
  // exponential distribution.
  vector<string> names;
  names.push_back(":authority");
  names.push_back(":path");
  names.push_back(":status");
  // TODO(jgraettinger): Enable "cookie" as a name fixture. Crumbs may be
  // reconstructed in any order, which breaks the simple validation used here.

  vector<string> values;
  values.push_back("/");
  values.push_back("/index.html");
  values.push_back("200");
  values.push_back("404");
  values.push_back("");
  values.push_back("baz=bing; foo=bar; garbage");
  values.push_back("baz=bing; fizzle=fazzle; garbage");

  int seed = std::time(NULL);
  LOG(INFO) << "Seeding with srand(" << seed << ")";
  srand(seed);

  for (size_t i = 0; i != 2000; ++i) {
    map<string, string> headers;

    size_t header_count = 1 + SampleExponential(7, 50);
    for (size_t j = 0; j != header_count; ++j) {
      size_t name_index = SampleExponential(20, 200);
      size_t value_index = SampleExponential(20, 200);

      string name, value;
      if (name_index >= names.size()) {
        names.push_back(base::RandBytesAsString(1 + SampleExponential(5, 30)));
        name = names.back();
      } else {
        name = names[name_index];
      }
      if (value_index >= values.size()) {
        values.push_back(base::RandBytesAsString(
            1 + SampleExponential(15, 75)));
        value = values.back();
      } else {
        value = values[value_index];
      }
      headers[name] = value;
    }
    EXPECT_TRUE(RoundTrip(headers));
  }
}

}  // namespace

}  // namespace net
