// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http_request.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test_server {

TEST(HttpRequestTest, ParseRequest) {
  HttpRequestParser parser;

  // Process request in chunks to check if the parser deals with border cases.
  // Also, check multi-line headers as well as multiple requests in the same
  // chunk. This basically should cover all the simplest border cases.
  parser.ProcessChunk("POST /foobar.html HTTP/1.1\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  parser.ProcessChunk("Host: localhost:1234\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  parser.ProcessChunk("Multi-line-header: abcd\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  parser.ProcessChunk(" efgh\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  parser.ProcessChunk(" ijkl\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  parser.ProcessChunk("Content-Length: 10\r\n\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  // Content data and another request in the same chunk (possible in http/1.1).
  parser.ProcessChunk("1234567890GET /another.html HTTP/1.1\r\n\r\n");
  ASSERT_EQ(HttpRequestParser::ACCEPTED, parser.ParseRequest());

  // Fetch the first request and validate it.
  {
    scoped_ptr<HttpRequest> request = parser.GetRequest();
    EXPECT_EQ("/foobar.html", request->relative_url);
    EXPECT_EQ(METHOD_POST, request->method);
    EXPECT_EQ("1234567890", request->content);
    ASSERT_EQ(3u, request->headers.size());

    EXPECT_EQ(1u, request->headers.count("Host"));
    EXPECT_EQ(1u, request->headers.count("Multi-line-header"));
    EXPECT_EQ(1u, request->headers.count("Content-Length"));

    EXPECT_EQ("localhost:1234", request->headers["Host"]);
    EXPECT_EQ("abcd efgh ijkl", request->headers["Multi-line-header"]);
    EXPECT_EQ("10", request->headers["Content-Length"]);
  }

  // No other request available yet since we do not support multiple requests
  // per connection.
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
}

TEST(HttpRequestTest, ParseRequestWithEmptyBody) {
  HttpRequestParser parser;

  parser.ProcessChunk("POST /foobar.html HTTP/1.1\r\n");
  parser.ProcessChunk("Content-Length: 0\r\n\r\n");
  ASSERT_EQ(HttpRequestParser::ACCEPTED, parser.ParseRequest());

  scoped_ptr<HttpRequest> request = parser.GetRequest();
  EXPECT_EQ("", request->content);
  EXPECT_TRUE(request->has_content);
  EXPECT_EQ(1u, request->headers.count("Content-Length"));
  EXPECT_EQ("0", request->headers["Content-Length"]);
}

TEST(HttpRequestTest, ParseRequestWithoutBody) {
  HttpRequestParser parser;

  parser.ProcessChunk("POST /foobar.html HTTP/1.1\r\n\r\n");
  ASSERT_EQ(HttpRequestParser::ACCEPTED, parser.ParseRequest());

  scoped_ptr<HttpRequest> request = parser.GetRequest();
  EXPECT_EQ("", request->content);
  EXPECT_FALSE(request->has_content);
}

}  // namespace test_server
}  // namespace net
