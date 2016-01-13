// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_spdy_client_stream.h"

#include "base/strings/string_number_conversions.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/quic_client_session.h"
#include "net/tools/quic/quic_spdy_client_stream.h"
#include "net/tools/quic/spdy_utils.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::DefaultQuicConfig;
using net::test::SupportedVersions;
using testing::StrictMock;
using testing::TestWithParam;

namespace net {
namespace tools {
namespace test {
namespace {

class QuicSpdyClientStreamTest : public TestWithParam<QuicVersion> {
 public:
  QuicSpdyClientStreamTest()
      : connection_(new StrictMock<MockConnection>(
            false, SupportedVersions(GetParam()))),
        session_(QuicServerId("example.com", 80, false, PRIVACY_MODE_DISABLED),
                 DefaultQuicConfig(),
                 connection_,
                 &crypto_config_),
        body_("hello world") {
    crypto_config_.SetDefaults();

    headers_.SetResponseFirstlineFromStringPieces("HTTP/1.1", "200", "Ok");
    headers_.ReplaceOrAppendHeader("content-length", "11");

    headers_string_ = SpdyUtils::SerializeResponseHeaders(headers_);

    // New streams rely on having the peer's flow control receive window
    // negotiated in the config.
    session_.config()->SetInitialFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    session_.config()->SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    session_.config()->SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    stream_.reset(new QuicSpdyClientStream(3, &session_));
  }

  StrictMock<MockConnection>* connection_;
  QuicClientSession session_;
  scoped_ptr<QuicSpdyClientStream> stream_;
  BalsaHeaders headers_;
  string headers_string_;
  string body_;
  QuicCryptoClientConfig crypto_config_;
};

INSTANTIATE_TEST_CASE_P(Tests, QuicSpdyClientStreamTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicSpdyClientStreamTest, TestFraming) {
  EXPECT_EQ(headers_string_.size(), stream_->ProcessData(
      headers_string_.c_str(), headers_string_.size()));
  EXPECT_EQ(body_.size(),
            stream_->ProcessData(body_.c_str(), body_.size()));
  EXPECT_EQ(200u, stream_->headers().parsed_response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_P(QuicSpdyClientStreamTest, TestFramingOnePacket) {
  string message = headers_string_ + body_;

  EXPECT_EQ(message.size(), stream_->ProcessData(
      message.c_str(), message.size()));
  EXPECT_EQ(200u, stream_->headers().parsed_response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_P(QuicSpdyClientStreamTest, DISABLED_TestFramingExtraData) {
  string large_body = "hello world!!!!!!";

  EXPECT_EQ(headers_string_.size(), stream_->ProcessData(
      headers_string_.c_str(), headers_string_.size()));
  // The headers should parse successfully.
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(200u, stream_->headers().parsed_response_code());

  EXPECT_CALL(*connection_,
              SendRstStream(stream_->id(), QUIC_BAD_APPLICATION_PAYLOAD, 0));
  stream_->ProcessData(large_body.c_str(), large_body.size());

  EXPECT_NE(QUIC_STREAM_NO_ERROR, stream_->stream_error());
}

TEST_P(QuicSpdyClientStreamTest, TestNoBidirectionalStreaming) {
  QuicStreamFrame frame(3, false, 3, MakeIOVector("asd"));

  EXPECT_FALSE(stream_->write_side_closed());
  EXPECT_TRUE(stream_->OnStreamFrame(frame));
  EXPECT_TRUE(stream_->write_side_closed());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
