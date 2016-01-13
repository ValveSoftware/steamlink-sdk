// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_extension_parser.h"

#include <string>

#include "net/websockets/websocket_extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(WebSocketExtensionParserTest, ParseEmpty) {
  WebSocketExtensionParser parser;
  parser.Parse("", 0);

  EXPECT_TRUE(parser.has_error());
}

TEST(WebSocketExtensionParserTest, ParseSimple) {
  WebSocketExtensionParser parser;
  WebSocketExtension expected("foo");

  parser.Parse("foo");

  ASSERT_FALSE(parser.has_error());
  EXPECT_TRUE(expected.Equals(parser.extension()));
}

TEST(WebSocketExtensionParserTest, ParseOneExtensionWithOneParamWithoutValue) {
  WebSocketExtensionParser parser;
  WebSocketExtension expected("foo");
  expected.Add(WebSocketExtension::Parameter("bar"));

  parser.Parse("\tfoo ; bar");

  ASSERT_FALSE(parser.has_error());
  EXPECT_TRUE(expected.Equals(parser.extension()));
}

TEST(WebSocketExtensionParserTest, ParseOneExtensionWithOneParamWithValue) {
  WebSocketExtensionParser parser;
  WebSocketExtension expected("foo");
  expected.Add(WebSocketExtension::Parameter("bar", "baz"));

  parser.Parse("foo ; bar= baz\t");

  ASSERT_FALSE(parser.has_error());
  EXPECT_TRUE(expected.Equals(parser.extension()));
}

TEST(WebSocketExtensionParserTest, ParseOneExtensionWithParams) {
  WebSocketExtensionParser parser;
  WebSocketExtension expected("foo");
  expected.Add(WebSocketExtension::Parameter("bar", "baz"));
  expected.Add(WebSocketExtension::Parameter("hoge", "fuga"));

  parser.Parse("foo ; bar= baz;\t \thoge\t\t=fuga");

  ASSERT_FALSE(parser.has_error());
  EXPECT_TRUE(expected.Equals(parser.extension()));
}

TEST(WebSocketExtensionParserTest, InvalidPatterns) {
  const char* patterns[] = {
    "fo\ao",  // control in extension name
    "fo\x01o",  // control in extension name
    "fo<o",  // separator in extension name
    "foo/",  // separator in extension name
    ";bar",  // empty extension name
    "foo bar",  // missing ';'
    "foo;",  // extension parameter without name and value
    "foo; b\ar",  // control in parameter name
    "foo; b\x7fr",  // control in parameter name
    "foo; b[r",  // separator in parameter name
    "foo; ba:",  // separator in parameter name
    "foo; =baz",  // empty parameter name
    "foo; bar=",  // empty parameter value
    "foo; =",  // empty parameter name and value
    "foo; bar=b\x02z",  // control in parameter value
    "foo; bar=b@z",  // separator in parameter value
    "foo; bar=b\\z",  // separator in parameter value
    "foo; bar=b?z",  // separator in parameter value
    "\"foo\"",  // quoted extension name
    "foo; \"bar\"",  // quoted parameter name
    "foo; bar=\"\a2\"",  // control in quoted parameter value
    "foo; bar=\"b@z\"",  // separator in quoted parameter value
    "foo; bar=\"b\\\\z\"",  // separator in quoted parameter value
    "foo; bar=\"\"",  // quoted empty parameter value
    "foo; bar=\"baz",  // unterminated quoted string
    "foo; bar=\"baz \"",  // space in quoted string
    "foo; bar baz",  // mising '='
    "foo; bar - baz",  // '-' instead of '=' (note: "foo; bar-baz" is valid).
    "foo; bar=\r\nbaz",  // CRNL not followed by a space
    "foo; bar=\r\n baz",  // CRNL followed by a space
    "foo, bar"  // multiple extensions
  };

  for (size_t i = 0; i < arraysize(patterns); ++i) {
    WebSocketExtensionParser parser;
    parser.Parse(patterns[i]);
    EXPECT_TRUE(parser.has_error());
  }
}

TEST(WebSocketExtensionParserTest, QuotedParameterValue) {
  WebSocketExtensionParser parser;
  WebSocketExtension expected("foo");
  expected.Add(WebSocketExtension::Parameter("bar", "baz"));

  parser.Parse("foo; bar = \"ba\\z\" ");

  ASSERT_FALSE(parser.has_error());
  EXPECT_TRUE(expected.Equals(parser.extension()));
}

}  // namespace

}  // namespace net
