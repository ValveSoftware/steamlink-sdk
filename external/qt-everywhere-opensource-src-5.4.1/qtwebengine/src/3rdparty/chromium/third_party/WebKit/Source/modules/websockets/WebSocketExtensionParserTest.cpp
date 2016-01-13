/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/websockets/WebSocketExtensionParser.h"

#include "wtf/HashMap.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

TEST(WebSocketExtensionParserTest, simpleExtension)
{
    CString input("extension");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_TRUE(parser.parseExtension(extension, parameters));
    EXPECT_EQ("extension", extension);
    EXPECT_EQ(0UL, parameters.size());
}

TEST(WebSocketExtensionParserTest, extensionWithParameters)
{
    CString input("extension;  foo=FOO; bar=BAR; baz");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_TRUE(parser.parseExtension(extension, parameters));
    EXPECT_EQ("extension", extension);
    EXPECT_EQ(3UL, parameters.size());
    EXPECT_EQ("FOO", parameters.get("foo"));
    EXPECT_EQ("BAR", parameters.get("bar"));
    EXPECT_TRUE(parameters.contains("baz"));
    EXPECT_TRUE(parameters.get("baz").isNull());
}

TEST(WebSocketExtensionParserTest, extensionWithQuotedParameter)
{
    CString input("extension; foo=FOO;  bar=\"BA\\R\"");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_TRUE(parser.parseExtension(extension, parameters));
    EXPECT_EQ("extension", extension);
    EXPECT_EQ(2UL, parameters.size());
    EXPECT_EQ("FOO", parameters.get("foo"));
    EXPECT_EQ("BAR", parameters.get("bar"));
}

TEST(WebSocketExtensionParserTest, colonSeparated)
{
    CString input("extension: foo=FOO");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_FALSE(parser.parseExtension(extension, parameters));
}

TEST(WebSocketExtensionParserTest, emptyTokenParameter)
{
    CString input("extension; foo=");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_FALSE(parser.parseExtension(extension, parameters));
}

TEST(WebSocketExtensionParserTest, emptyQuotedParameter)
{
    CString input("extension; foo=\"\"");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_FALSE(parser.parseExtension(extension, parameters));
}

TEST(WebSocketExtensionParserTest, non7bitAsciiInExtensionName)
{
    CString input("exten\xe0sion");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_FALSE(parser.parseExtension(extension, parameters));
}

TEST(WebSocketExtensionParserTest, controlCharacterInExtensionName)
{
    CString input("exten\bsion");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_FALSE(parser.parseExtension(extension, parameters));
}

TEST(WebSocketExtensionParserTest, separatorInExtensionName)
{
    CString input("exten\tsion");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_FALSE(parser.parseExtension(extension, parameters));
}

TEST(WebSocketExtensionParserTest, unterminatedQuote)
{
    CString input("extension; foo=\"FOO");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_FALSE(parser.parseExtension(extension, parameters));
}

TEST(WebSocketExtensionParserTest, non7bitAsciiInQuotedParameter)
{
    CString input("extension; foo=\"FO\xffO\"");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_FALSE(parser.parseExtension(extension, parameters));
}

TEST(WebSocketExtensionParserTest, controlCharacterInQuotedParameter)
{
    CString input("extension; foo=\"FO\bO\"");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_FALSE(parser.parseExtension(extension, parameters));
}

TEST(WebSocketExtensionParserTest, separatorInQuotedParameter)
{
    CString input("extension; foo=\"FO O\"");
    String extension;
    HashMap<String, String> parameters;
    WebSocketExtensionParser parser(input.data(), input.data() + input.length());

    ASSERT_FALSE(parser.parseExtension(extension, parameters));
}

} // namespace
