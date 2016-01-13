// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_spdy_server_stream.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/spdy_utils.h"
#include "net/tools/quic/test_tools/quic_in_memory_cache_peer.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using net::test::MockSession;
using net::test::SupportedVersions;
using net::tools::test::MockConnection;
using std::string;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::InvokeArgument;
using testing::InSequence;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;

namespace net {
namespace tools {
namespace test {

class QuicSpdyServerStreamPeer : public QuicSpdyServerStream {
 public:
  QuicSpdyServerStreamPeer(QuicStreamId stream_id, QuicSession* session)
      : QuicSpdyServerStream(stream_id, session) {
  }

  using QuicSpdyServerStream::SendResponse;
  using QuicSpdyServerStream::SendErrorResponse;

  BalsaHeaders* mutable_headers() {
    return &headers_;
  }

  static void SendResponse(QuicSpdyServerStream* stream) {
    stream->SendResponse();
  }

  static void SendErrorResponse(QuicSpdyServerStream* stream) {
    stream->SendResponse();
  }

  static const string& body(QuicSpdyServerStream* stream) {
    return stream->body_;
  }

  static const BalsaHeaders& headers(QuicSpdyServerStream* stream) {
    return stream->headers_;
  }
};

namespace {

class QuicSpdyServerStreamTest : public ::testing::TestWithParam<QuicVersion> {
 public:
  QuicSpdyServerStreamTest()
      : connection_(new StrictMock<MockConnection>(
            true, SupportedVersions(GetParam()))),
        session_(connection_),
        body_("hello world") {
    BalsaHeaders request_headers;
    request_headers.SetRequestFirstlineFromStringPieces(
        "POST", "https://www.google.com/", "HTTP/1.1");
    request_headers.ReplaceOrAppendHeader("content-length", "11");

    headers_string_ = SpdyUtils::SerializeRequestHeaders(request_headers);

    // New streams rely on having the peer's flow control receive window
    // negotiated in the config.
    session_.config()->SetInitialFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    session_.config()->SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    session_.config()->SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    stream_.reset(new QuicSpdyServerStreamPeer(3, &session_));
  }

  static void SetUpTestCase() {
    QuicInMemoryCachePeer::ResetForTests();
  }

  virtual void SetUp() OVERRIDE {
    QuicInMemoryCache* cache = QuicInMemoryCache::GetInstance();

    BalsaHeaders request_headers, response_headers;
    StringPiece body("Yum");
    request_headers.SetRequestFirstlineFromStringPieces(
        "GET",
        "https://www.google.com/foo",
        "HTTP/1.1");
    response_headers.SetRequestFirstlineFromStringPieces("HTTP/1.1",
                                                         "200",
                                                         "OK");
    response_headers.AppendHeader("content-length",
                                  base::IntToString(body.length()));

    // Check if response already exists and matches.
    const QuicInMemoryCache::Response* cached_response =
        cache->GetResponse(request_headers);
    if (cached_response != NULL) {
      string cached_response_headers_str, response_headers_str;
      cached_response->headers().DumpToString(&cached_response_headers_str);
      response_headers.DumpToString(&response_headers_str);
      CHECK_EQ(cached_response_headers_str, response_headers_str);
      CHECK_EQ(cached_response->body(), body);
      return;
    }

    cache->AddResponse(request_headers, response_headers, body);
  }

  const string& StreamBody() {
    return QuicSpdyServerStreamPeer::body(stream_.get());
  }

  const BalsaHeaders& StreamHeaders() {
    return QuicSpdyServerStreamPeer::headers(stream_.get());
  }

  BalsaHeaders response_headers_;
  EpollServer eps_;
  StrictMock<MockConnection>* connection_;
  StrictMock<MockSession> session_;
  scoped_ptr<QuicSpdyServerStreamPeer> stream_;
  string headers_string_;
  string body_;
};

QuicConsumedData ConsumeAllData(
    QuicStreamId id,
    const IOVector& data,
    QuicStreamOffset offset,
    bool fin,
    FecProtection /*fec_protection_*/,
    QuicAckNotifier::DelegateInterface* /*ack_notifier_delegate*/) {
  return QuicConsumedData(data.TotalBufferSize(), fin);
}

INSTANTIATE_TEST_CASE_P(Tests, QuicSpdyServerStreamTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicSpdyServerStreamTest, TestFraming) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _)).Times(AnyNumber()).
      WillRepeatedly(Invoke(ConsumeAllData));

  EXPECT_EQ(headers_string_.size(), stream_->ProcessData(
      headers_string_.c_str(), headers_string_.size()));
  EXPECT_EQ(body_.size(), stream_->ProcessData(body_.c_str(), body_.size()));
  EXPECT_EQ(11u, StreamHeaders().content_length());
  EXPECT_EQ("https://www.google.com/", StreamHeaders().request_uri());
  EXPECT_EQ("POST", StreamHeaders().request_method());
  EXPECT_EQ(body_, StreamBody());
}

TEST_P(QuicSpdyServerStreamTest, TestFramingOnePacket) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _)).Times(AnyNumber()).
      WillRepeatedly(Invoke(ConsumeAllData));

  string message = headers_string_ + body_;

  EXPECT_EQ(message.size(), stream_->ProcessData(
      message.c_str(), message.size()));
  EXPECT_EQ(11u, StreamHeaders().content_length());
  EXPECT_EQ("https://www.google.com/", StreamHeaders().request_uri());
  EXPECT_EQ("POST", StreamHeaders().request_method());
  EXPECT_EQ(body_, StreamBody());
}

TEST_P(QuicSpdyServerStreamTest, TestFramingExtraData) {
  string large_body = "hello world!!!!!!";

  // We'll automatically write out an error (headers + body)
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _)).Times(AnyNumber()).
      WillRepeatedly(Invoke(ConsumeAllData));

  EXPECT_EQ(headers_string_.size(), stream_->ProcessData(
      headers_string_.c_str(), headers_string_.size()));
  // Content length is still 11.  This will register as an error and we won't
  // accept the bytes.
  stream_->ProcessData(large_body.c_str(), large_body.size());
  EXPECT_EQ(11u, StreamHeaders().content_length());
  EXPECT_EQ("https://www.google.com/", StreamHeaders().request_uri());
  EXPECT_EQ("POST", StreamHeaders().request_method());
}

TEST_P(QuicSpdyServerStreamTest, TestSendResponse) {
  BalsaHeaders* request_headers = stream_->mutable_headers();
  request_headers->SetRequestFirstlineFromStringPieces(
      "GET",
      "https://www.google.com/foo",
      "HTTP/1.1");

  response_headers_.SetResponseFirstlineFromStringPieces(
      "HTTP/1.1", "200", "OK");
  response_headers_.ReplaceOrAppendHeader("content-length", "3");

  InSequence s;
  EXPECT_CALL(session_,
              WritevData(kHeadersStreamId, _, 0, false, _, NULL));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _)).Times(1).
      WillOnce(Return(QuicConsumedData(3, true)));

  QuicSpdyServerStreamPeer::SendResponse(stream_.get());
  EXPECT_TRUE(stream_->read_side_closed());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSpdyServerStreamTest, TestSendErrorResponse) {
  response_headers_.SetResponseFirstlineFromStringPieces(
      "HTTP/1.1", "500", "Server Error");
  response_headers_.ReplaceOrAppendHeader("content-length", "3");

  InSequence s;
  EXPECT_CALL(session_,
              WritevData(kHeadersStreamId, _, 0, false, _, NULL));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _)).Times(1).
      WillOnce(Return(QuicConsumedData(3, true)));

  QuicSpdyServerStreamPeer::SendErrorResponse(stream_.get());
  EXPECT_TRUE(stream_->read_side_closed());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSpdyServerStreamTest, InvalidHeadersWithFin) {
  char arr[] = {
    0x3a, 0x68, 0x6f, 0x73,  // :hos
    0x74, 0x00, 0x00, 0x00,  // t...
    0x00, 0x00, 0x00, 0x00,  // ....
    0x07, 0x3a, 0x6d, 0x65,  // .:me
    0x74, 0x68, 0x6f, 0x64,  // thod
    0x00, 0x00, 0x00, 0x03,  // ....
    0x47, 0x45, 0x54, 0x00,  // GET.
    0x00, 0x00, 0x05, 0x3a,  // ...:
    0x70, 0x61, 0x74, 0x68,  // path
    0x00, 0x00, 0x00, 0x04,  // ....
    0x2f, 0x66, 0x6f, 0x6f,  // /foo
    0x00, 0x00, 0x00, 0x07,  // ....
    0x3a, 0x73, 0x63, 0x68,  // :sch
    0x65, 0x6d, 0x65, 0x00,  // eme.
    0x00, 0x00, 0x00, 0x00,  // ....
    0x00, 0x00, 0x08, 0x3a,  // ...:
    0x76, 0x65, 0x72, 0x73,  // vers
    '\x96', 0x6f, 0x6e, 0x00,  // <i(69)>on.
    0x00, 0x00, 0x08, 0x48,  // ...H
    0x54, 0x54, 0x50, 0x2f,  // TTP/
    0x31, 0x2e, 0x31,        // 1.1
  };
  StringPiece data(arr, arraysize(arr));
  QuicStreamFrame frame(stream_->id(), true, 0, MakeIOVector(data));
  // Verify that we don't crash when we get a invalid headers in stream frame.
  stream_->OnStreamFrame(frame);
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
