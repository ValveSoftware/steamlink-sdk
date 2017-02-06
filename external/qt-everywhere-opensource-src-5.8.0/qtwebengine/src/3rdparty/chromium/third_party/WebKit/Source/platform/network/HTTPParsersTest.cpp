// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/HTTPParsers.h"

#include "platform/heap/Handle.h"
#include "platform/weborigin/Suborigin.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/MathExtras.h"
#include "wtf/dtoa/utils.h"
#include "wtf/text/AtomicString.h"

namespace blink {

TEST(HTTPParsersTest, ParseCacheControl)
{
    CacheControlHeader header;

    header = parseCacheControlDirectives("no-cache", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_TRUE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("no-cache no-store", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_TRUE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("no-store must-revalidate", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_TRUE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("max-age=0", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_EQ(0.0, header.maxAge);

    header = parseCacheControlDirectives("max-age", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("max-age=0 no-cache", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_EQ(0.0, header.maxAge);

    header = parseCacheControlDirectives("no-cache=foo", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("nonsense", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("\rno-cache\n\t\v\0\b", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_TRUE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("      no-cache       ", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_TRUE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives(AtomicString(), "no-cache");
    EXPECT_TRUE(header.parsed);
    EXPECT_TRUE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));
}

TEST(HTTPParsersTest, CommaDelimitedHeaderSet)
{
    CommaDelimitedHeaderSet set1;
    CommaDelimitedHeaderSet set2;
    parseCommaDelimitedHeader("dpr, rw, whatever", set1);
    EXPECT_TRUE(set1.contains("dpr"));
    EXPECT_TRUE(set1.contains("rw"));
    EXPECT_TRUE(set1.contains("whatever"));
    parseCommaDelimitedHeader("dprw\t     , fo\to", set2);
    EXPECT_FALSE(set2.contains("dpr"));
    EXPECT_FALSE(set2.contains("rw"));
    EXPECT_FALSE(set2.contains("whatever"));
    EXPECT_TRUE(set2.contains("dprw"));
    EXPECT_FALSE(set2.contains("foo"));
    EXPECT_TRUE(set2.contains("fo\to"));
}

TEST(HTTPParsersTest, HTTPFieldContent)
{
    const UChar hiraganaA[2] = { 0x3042, 0 };

    EXPECT_TRUE(blink::isValidHTTPFieldContentRFC7230("\xd0\xa1"));
    EXPECT_TRUE(blink::isValidHTTPFieldContentRFC7230("t t"));
    EXPECT_TRUE(blink::isValidHTTPFieldContentRFC7230("t\tt"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(" "));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(""));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("\x7f"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("t\rt"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("t\nt"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("t\bt"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("t\vt"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(" t"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("t "));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(String("t\0t", 3)));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(String("\0", 1)));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(String("test \0, 6")));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(String("test ")));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("test\r\n data"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(String(hiraganaA)));
}

TEST(HTTPParsersTest, ExtractMIMETypeFromMediaType)
{
    const AtomicString textHtml("text/html");
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("text/html; charset=iso-8859-1")));
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("text/html ; charset=iso-8859-1")));
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("text/html,text/plain")));
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("text/html , text/plain")));
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("text/html\t,\ttext/plain")));
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString(" text/html   ")));
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("\ttext/html \t")));
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("\r\ntext/html\r\n")));
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("text/html,text/plain;charset=iso-8859-1")));
    EXPECT_EQ(emptyString(), extractMIMETypeFromMediaType(AtomicString(", text/html")));
    EXPECT_EQ(emptyString(), extractMIMETypeFromMediaType(AtomicString("; text/html")));

    // Preserves case.
    EXPECT_EQ("tExt/hTMl", extractMIMETypeFromMediaType(AtomicString("tExt/hTMl")));

    // If no normalization is required, the same AtomicString should be returned.
    const AtomicString& passthrough = extractMIMETypeFromMediaType(textHtml);
    EXPECT_EQ(textHtml.impl(), passthrough.impl());

    // These tests cover current behavior, but are not necessarily
    // expected/wanted behavior. (See FIXME in implementation.)
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("text / html")));
    // U+2003, EM SPACE (UTF-8: E2 80 83)
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString::fromUTF8("text\xE2\x80\x83/ html")));
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("text\r\n/\nhtml")));
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("text\n/\nhtml")));
    EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("t e x t / h t m l")));
}

void expectParseNamePass(const char* message, String header, String expectedName)
{
    SCOPED_TRACE(message);

    Vector<String> messages;
    Suborigin suborigin;
    EXPECT_TRUE(parseSuboriginHeader(header, &suborigin, messages));
    EXPECT_EQ(expectedName, suborigin.name());
}

void expectParseNameFail(const char* message, String header)
{
    SCOPED_TRACE(message);

    Vector<String> messages;
    Suborigin suborigin;
    EXPECT_FALSE(parseSuboriginHeader(header, &suborigin, messages));
    EXPECT_EQ(String(), suborigin.name());
}

void expectParsePolicyPass(const char* message, String header, const Suborigin::SuboriginPolicyOptions expectedPolicy[], size_t numPolicies)
{
    SCOPED_TRACE(message);

    Vector<String> messages;
    Suborigin suborigin;
    EXPECT_TRUE(parseSuboriginHeader(header, &suborigin, messages));
    unsigned policiesMask = 0;
    for (size_t i = 0; i < numPolicies; i++)
        policiesMask |= static_cast<unsigned>(expectedPolicy[i]);
    EXPECT_EQ(policiesMask, suborigin.optionsMask());
}

void expectParsePolicyFail(const char* message, String header)
{
    SCOPED_TRACE(message);

    Vector<String> messages;
    Suborigin suborigin;
    EXPECT_FALSE(parseSuboriginHeader(header, &suborigin, messages));
    EXPECT_EQ(String(), suborigin.name());
}

TEST(HTTPParsersTest, SuboriginParseValidNames)
{
    // Single headers
    expectParseNamePass("Alpha", "foo", "foo");
    expectParseNamePass("Whitespace alpha", "  foo  ", "foo");
    expectParseNamePass("Alphanumeric", "f0o", "f0o");
    expectParseNamePass("Numeric", "42", "42");
    expectParseNamePass("Hyphen middle", "foo-bar", "foo-bar");
    expectParseNamePass("Hyphen start", "-foobar", "-foobar");
    expectParseNamePass("Hyphen end", "foobar-", "foobar-");

    // Mulitple headers should only give the first name
    expectParseNamePass("Multiple headers, no whitespace", "foo,bar", "foo");
    expectParseNamePass("Multiple headers, whitespace before second", "foo, bar", "foo");
    expectParseNamePass("Multiple headers, whitespace after first and before second", "foo, bar", "foo");
    expectParseNamePass("Multiple headers, empty second ignored", "foo, bar", "foo");
    expectParseNamePass("Multiple headers, invalid second ignored", "foo, bar", "foo");
}

TEST(HTTPParsersTest, SuboriginParseInvalidNames)
{
    // Single header, invalid value
    expectParseNameFail("Empty header", "");
    expectParseNameFail("Whitespace in middle", "foo bar");
    expectParseNameFail("Invalid character at end of name", "foobar'");
    expectParseNameFail("Invalid character at start of name", "'foobar");
    expectParseNameFail("Invalid character in middle of name", "foo'bar");
    expectParseNameFail("Alternate invalid character in middle of name", "foob@r");
    expectParseNameFail("First cap", "Foo");
    expectParseNameFail("All cap", "FOO");

    // Multiple headers, invalid value(s)
    expectParseNameFail("Multple headers, empty first header", ", bar");
    expectParseNameFail("Multple headers, both empty headers", ",");
    expectParseNameFail("Multple headers, invalid character in first header", "f@oo, bar");
    expectParseNameFail("Multple headers, invalid character in both headers", "f@oo, b@r");
}

TEST(HTTPParsersTest, SuboriginParseValidPolicy)
{
    const Suborigin::SuboriginPolicyOptions unsafePostmessageSend[] = { Suborigin::SuboriginPolicyOptions::UnsafePostMessageSend };
    const Suborigin::SuboriginPolicyOptions unsafePostmessageReceive[] = { Suborigin::SuboriginPolicyOptions::UnsafePostMessageReceive };
    const Suborigin::SuboriginPolicyOptions unsafePostmessageSendAndReceive[] = {  Suborigin::SuboriginPolicyOptions::UnsafePostMessageSend, Suborigin::SuboriginPolicyOptions::UnsafePostMessageReceive };
    const Suborigin::SuboriginPolicyOptions unsafeCookies[] = { Suborigin::SuboriginPolicyOptions::UnsafeCookies };
    const Suborigin::SuboriginPolicyOptions unsafeAllOptions[] = { Suborigin::SuboriginPolicyOptions::UnsafePostMessageSend, Suborigin::SuboriginPolicyOptions::UnsafePostMessageReceive, Suborigin::SuboriginPolicyOptions::UnsafeCookies };

    // All simple, valid policies
    expectParsePolicyPass("One policy, unsafe-postmessage-send", "foobar 'unsafe-postmessage-send'", unsafePostmessageSend, ARRAY_SIZE(unsafePostmessageSend));
    expectParsePolicyPass("One policy, unsafe-postmessage-receive", "foobar 'unsafe-postmessage-receive'", unsafePostmessageReceive, ARRAY_SIZE(unsafePostmessageReceive));
    expectParsePolicyPass("One policy, unsafe-cookies", "foobar 'unsafe-cookies'", unsafeCookies, ARRAY_SIZE(unsafeCookies));

    // Formatting differences of policies and multiple policies
    expectParsePolicyPass("One policy, whitespace all around", "foobar      'unsafe-postmessage-send'          ", unsafePostmessageSend, ARRAY_SIZE(unsafePostmessageSend));
    expectParsePolicyPass("Multiple, same policies", "foobar 'unsafe-postmessage-send' 'unsafe-postmessage-send'", unsafePostmessageSend, ARRAY_SIZE(unsafePostmessageSend));
    expectParsePolicyPass("Multiple, different policies", "foobar 'unsafe-postmessage-send' 'unsafe-postmessage-receive'", unsafePostmessageSendAndReceive, ARRAY_SIZE(unsafePostmessageSendAndReceive));
    expectParsePolicyPass("Many different policies", "foobar 'unsafe-postmessage-send' 'unsafe-postmessage-receive' 'unsafe-cookies'", unsafeAllOptions, ARRAY_SIZE(unsafeAllOptions));
    expectParsePolicyPass("One policy, unknown option", "foobar 'unknown-option'", {}, 0);
}

TEST(HTTPParsersTest, SuboriginParseInvalidPolicy)
{
    expectParsePolicyFail("One policy, no suborigin name", "'unsafe-postmessage-send'");
    expectParsePolicyFail("One policy, invalid characters", "foobar 'un$afe-postmessage-send'");
    expectParsePolicyFail("One policy, caps", "foobar 'UNSAFE-POSTMESSAGE-SEND'");
    expectParsePolicyFail("One policy, missing first quote", "foobar unsafe-postmessage-send'");
    expectParsePolicyFail("One policy, missing last quote", "foobar 'unsafe-postmessage-send");
    expectParsePolicyFail("One policy, invalid character at end", "foobar 'unsafe-postmessage-send';");
    expectParsePolicyFail("Multiple policies, extra character between options", "foobar 'unsafe-postmessage-send' ; 'unsafe-postmessage-send'");
    expectParsePolicyFail("Policy that is a single quote", "foobar '");
    expectParsePolicyFail("Valid policy and then policy that is a single quote", "foobar 'unsafe-postmessage-send' '");
}

} // namespace blink
