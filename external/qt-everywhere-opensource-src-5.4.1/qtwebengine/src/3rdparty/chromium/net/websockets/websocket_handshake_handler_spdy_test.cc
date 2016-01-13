// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_handler.h"

#include <string>

#include "net/socket/next_proto.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_websocket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

class WebSocketHandshakeHandlerSpdyTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<NextProto> {
 protected:
  WebSocketHandshakeHandlerSpdyTest() : spdy_util_(GetParam()) {}

  SpdyWebSocketTestUtil spdy_util_;
};

INSTANTIATE_TEST_CASE_P(
    NextProto,
    WebSocketHandshakeHandlerSpdyTest,
    testing::Values(kProtoDeprecatedSPDY2,
                    kProtoSPDY3, kProtoSPDY31, kProtoSPDY4));

TEST_P(WebSocketHandshakeHandlerSpdyTest, RequestResponse) {
  WebSocketHandshakeRequestHandler request_handler;

  static const char kHandshakeRequestMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Origin: http://example.com\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Sec-WebSocket-Extensions: foo\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "\r\n";

  EXPECT_TRUE(request_handler.ParseRequest(kHandshakeRequestMessage,
                                           strlen(kHandshakeRequestMessage)));

  GURL url("ws://example.com/demo");
  std::string challenge;
  SpdyHeaderBlock headers;
  ASSERT_TRUE(request_handler.GetRequestHeaderBlock(url,
                                                    &headers,
                                                    &challenge,
                                                    spdy_util_.spdy_version()));

  EXPECT_EQ(url.path(), spdy_util_.GetHeader(headers, "path"));
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "upgrade").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "Upgrade").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "connection").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "Connection").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "Sec-WebSocket-Key").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "sec-websocket-key").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "Sec-WebSocket-Version").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "sec-webSocket-version").empty());
  EXPECT_EQ("example.com", spdy_util_.GetHeader(headers, "host"));
  EXPECT_EQ("http://example.com", spdy_util_.GetHeader(headers, "origin"));
  EXPECT_EQ("sample", spdy_util_.GetHeader(headers, "sec-websocket-protocol"));
  EXPECT_EQ("foo", spdy_util_.GetHeader(headers, "sec-websocket-extensions"));
  EXPECT_EQ("ws", spdy_util_.GetHeader(headers, "scheme"));
  EXPECT_EQ("WebSocket/13", spdy_util_.GetHeader(headers, "version"));

  static const char expected_challenge[] = "dGhlIHNhbXBsZSBub25jZQ==";

  EXPECT_EQ(expected_challenge, challenge);

  headers.clear();

  spdy_util_.SetHeader("status", "101 Switching Protocols", &headers);
  spdy_util_.SetHeader("sec-websocket-protocol", "sample", &headers);
  spdy_util_.SetHeader("sec-websocket-extensions", "foo", &headers);

  WebSocketHandshakeResponseHandler response_handler;
  EXPECT_TRUE(response_handler.ParseResponseHeaderBlock(
      headers, challenge, spdy_util_.spdy_version()));
  EXPECT_TRUE(response_handler.HasResponse());

  // Note that order of sec-websocket-* is sensitive with hash_map order.
  static const char kHandshakeResponseExpectedMessage[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "sec-websocket-extensions: foo\r\n"
      "sec-websocket-protocol: sample\r\n"
      "\r\n";

  EXPECT_EQ(kHandshakeResponseExpectedMessage, response_handler.GetResponse());
}

TEST_P(WebSocketHandshakeHandlerSpdyTest, RequestResponseWithCookies) {
  WebSocketHandshakeRequestHandler request_handler;

  // Note that websocket won't use multiple headers in request now.
  static const char kHandshakeRequestMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Origin: http://example.com\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Sec-WebSocket-Extensions: foo\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "Cookie: WK-websocket-test=1; WK-websocket-test-httponly=1\r\n"
      "\r\n";

  EXPECT_TRUE(request_handler.ParseRequest(kHandshakeRequestMessage,
                                           strlen(kHandshakeRequestMessage)));

  GURL url("ws://example.com/demo");
  std::string challenge;
  SpdyHeaderBlock headers;
  ASSERT_TRUE(request_handler.GetRequestHeaderBlock(url,
                                                    &headers,
                                                    &challenge,
                                                    spdy_util_.spdy_version()));

  EXPECT_EQ(url.path(), spdy_util_.GetHeader(headers, "path"));
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "upgrade").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "Upgrade").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "connection").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "Connection").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "Sec-WebSocket-Key").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "sec-websocket-key").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "Sec-WebSocket-Version").empty());
  EXPECT_TRUE(spdy_util_.GetHeader(headers, "sec-webSocket-version").empty());
  EXPECT_EQ("example.com", spdy_util_.GetHeader(headers, "host"));
  EXPECT_EQ("http://example.com", spdy_util_.GetHeader(headers, "origin"));
  EXPECT_EQ("sample", spdy_util_.GetHeader(headers, "sec-websocket-protocol"));
  EXPECT_EQ("foo", spdy_util_.GetHeader(headers, "sec-websocket-extensions"));
  EXPECT_EQ("ws", spdy_util_.GetHeader(headers, "scheme"));
  EXPECT_EQ("WebSocket/13", spdy_util_.GetHeader(headers, "version"));
  EXPECT_EQ("WK-websocket-test=1; WK-websocket-test-httponly=1",
            headers["cookie"]);

  const char expected_challenge[] = "dGhlIHNhbXBsZSBub25jZQ==";

  EXPECT_EQ(expected_challenge, challenge);

  headers.clear();

  spdy_util_.SetHeader("status", "101 Switching Protocols", &headers);
  spdy_util_.SetHeader("sec-websocket-protocol", "sample", &headers);
  spdy_util_.SetHeader("sec-websocket-extensions", "foo", &headers);
  std::string cookie = "WK-websocket-test=1";
  cookie.append(1, '\0');
  cookie += "WK-websocket-test-httponly=1; HttpOnly";
  headers["set-cookie"] = cookie;


  WebSocketHandshakeResponseHandler response_handler;
  EXPECT_TRUE(response_handler.ParseResponseHeaderBlock(
      headers, challenge, spdy_util_.spdy_version()));
  EXPECT_TRUE(response_handler.HasResponse());

  // Note that order of sec-websocket-* is sensitive with hash_map order.
  static const char kHandshakeResponseExpectedMessage[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "sec-websocket-extensions: foo\r\n"
      "sec-websocket-protocol: sample\r\n"
      "set-cookie: WK-websocket-test=1\r\n"
      "set-cookie: WK-websocket-test-httponly=1; HttpOnly\r\n"
      "\r\n";

  EXPECT_EQ(kHandshakeResponseExpectedMessage, response_handler.GetResponse());
}

}  // namespace

}  // namespace net
