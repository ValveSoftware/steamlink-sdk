// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/HTTPParsers.h"

#include "platform/heap/Handle.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/Suborigin.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/MathExtras.h"
#include "wtf/dtoa/utils.h"
#include "wtf/text/AtomicString.h"

namespace blink {

TEST(HTTPParsersTest, ParseCacheControl) {
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

  header =
      parseCacheControlDirectives("no-store must-revalidate", AtomicString());
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

TEST(HTTPParsersTest, CommaDelimitedHeaderSet) {
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

TEST(HTTPParsersTest, HTTPFieldContent) {
  const UChar hiraganaA[2] = {0x3042, 0};

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

TEST(HTTPParsersTest, HTTPToken) {
  const UChar hiraganaA[2] = {0x3042, 0};
  const UChar latinCapitalAWithMacron[2] = {0x100, 0};

  EXPECT_TRUE(blink::isValidHTTPToken("gzip"));
  EXPECT_TRUE(blink::isValidHTTPToken("no-cache"));
  EXPECT_TRUE(blink::isValidHTTPToken("86400"));
  EXPECT_TRUE(blink::isValidHTTPToken("~"));
  EXPECT_FALSE(blink::isValidHTTPToken(""));
  EXPECT_FALSE(blink::isValidHTTPToken(" "));
  EXPECT_FALSE(blink::isValidHTTPToken("\t"));
  EXPECT_FALSE(blink::isValidHTTPToken("\x7f"));
  EXPECT_FALSE(blink::isValidHTTPToken("\xff"));
  EXPECT_FALSE(blink::isValidHTTPToken(String(latinCapitalAWithMacron)));
  EXPECT_FALSE(blink::isValidHTTPToken("t a"));
  EXPECT_FALSE(blink::isValidHTTPToken("()"));
  EXPECT_FALSE(blink::isValidHTTPToken("(foobar)"));
  EXPECT_FALSE(blink::isValidHTTPToken(String("\0", 1)));
  EXPECT_FALSE(blink::isValidHTTPToken(String(hiraganaA)));
}

TEST(HTTPParsersTest, ExtractMIMETypeFromMediaType) {
  const AtomicString textHtml("text/html");

  EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString("text/html")));
  EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(
                          AtomicString("text/html; charset=iso-8859-1")));

  // Quoted charset parameter
  EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(
                          AtomicString("text/html; charset=\"quoted\"")));

  // Multiple parameters
  EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(
                          AtomicString("text/html; charset=x; foo=bar")));

  // OWSes are trimmed.
  EXPECT_EQ(textHtml,
            extractMIMETypeFromMediaType(AtomicString(" text/html   ")));
  EXPECT_EQ(textHtml,
            extractMIMETypeFromMediaType(AtomicString("\ttext/html \t")));
  EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(
                          AtomicString("text/html ; charset=iso-8859-1")));

  // Non-standard multiple type/subtype listing using a comma as a separator
  // is accepted.
  EXPECT_EQ(textHtml,
            extractMIMETypeFromMediaType(AtomicString("text/html,text/plain")));
  EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(
                          AtomicString("text/html , text/plain")));
  EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(
                          AtomicString("text/html\t,\ttext/plain")));
  EXPECT_EQ(textHtml, extractMIMETypeFromMediaType(AtomicString(
                          "text/html,text/plain;charset=iso-8859-1")));

  // Preserves case.
  EXPECT_EQ("tExt/hTMl",
            extractMIMETypeFromMediaType(AtomicString("tExt/hTMl")));

  EXPECT_EQ(emptyString(),
            extractMIMETypeFromMediaType(AtomicString(", text/html")));
  EXPECT_EQ(emptyString(),
            extractMIMETypeFromMediaType(AtomicString("; text/html")));

  // If no normalization is required, the same AtomicString should be returned.
  const AtomicString& passthrough = extractMIMETypeFromMediaType(textHtml);
  EXPECT_EQ(textHtml.impl(), passthrough.impl());
}

TEST(HTTPParsersTest, ExtractMIMETypeFromMediaTypeInvalidInput) {
  // extractMIMETypeFromMediaType() returns the string before the first
  // semicolon after trimming OWSes at the head and the tail even if the
  // string doesn't conform to the media-type ABNF defined in the RFC 7231.

  // These behaviors could be fixed later when ready.

  // Non-OWS characters meaning space are not trimmed.
  EXPECT_EQ(AtomicString("\r\ntext/html\r\n"),
            extractMIMETypeFromMediaType(AtomicString("\r\ntext/html\r\n")));
  // U+2003, EM SPACE (UTF-8: E2 80 83).
  EXPECT_EQ(AtomicString::fromUTF8("\xE2\x80\x83text/html"),
            extractMIMETypeFromMediaType(
                AtomicString::fromUTF8("\xE2\x80\x83text/html")));

  // Invalid type/subtype.
  EXPECT_EQ(AtomicString("a"), extractMIMETypeFromMediaType(AtomicString("a")));

  // Invalid parameters.
  EXPECT_EQ(AtomicString("text/html"),
            extractMIMETypeFromMediaType(AtomicString("text/html;wow")));
  EXPECT_EQ(AtomicString("text/html"),
            extractMIMETypeFromMediaType(AtomicString("text/html;;;;;;")));
  EXPECT_EQ(AtomicString("text/html"),
            extractMIMETypeFromMediaType(AtomicString("text/html; = = = ")));

  // Only OWSes at either the beginning or the end of the type/subtype
  // portion.
  EXPECT_EQ(AtomicString("text / html"),
            extractMIMETypeFromMediaType(AtomicString("text / html")));
  EXPECT_EQ(AtomicString("t e x t / h t m l"),
            extractMIMETypeFromMediaType(AtomicString("t e x t / h t m l")));

  EXPECT_EQ(AtomicString("text\r\n/\nhtml"),
            extractMIMETypeFromMediaType(AtomicString("text\r\n/\nhtml")));
  EXPECT_EQ(AtomicString("text\n/\nhtml"),
            extractMIMETypeFromMediaType(AtomicString("text\n/\nhtml")));
  EXPECT_EQ(AtomicString::fromUTF8("text\xE2\x80\x83/html"),
            extractMIMETypeFromMediaType(
                AtomicString::fromUTF8("text\xE2\x80\x83/html")));
}

void expectParseNamePass(const char* message,
                         String header,
                         String expectedName) {
  SCOPED_TRACE(message);

  Vector<String> messages;
  Suborigin suborigin;
  EXPECT_TRUE(parseSuboriginHeader(header, &suborigin, messages));
  EXPECT_EQ(expectedName, suborigin.name());
}

void expectParseNameFail(const char* message, String header) {
  SCOPED_TRACE(message);

  Vector<String> messages;
  Suborigin suborigin;
  EXPECT_FALSE(parseSuboriginHeader(header, &suborigin, messages));
  EXPECT_EQ(String(), suborigin.name());
}

void expectParsePolicyPass(
    const char* message,
    String header,
    const Suborigin::SuboriginPolicyOptions expectedPolicy[],
    size_t numPolicies) {
  SCOPED_TRACE(message);

  Vector<String> messages;
  Suborigin suborigin;
  EXPECT_TRUE(parseSuboriginHeader(header, &suborigin, messages));
  unsigned policiesMask = 0;
  for (size_t i = 0; i < numPolicies; i++)
    policiesMask |= static_cast<unsigned>(expectedPolicy[i]);
  EXPECT_EQ(policiesMask, suborigin.optionsMask());
}

void expectParsePolicyFail(const char* message, String header) {
  SCOPED_TRACE(message);

  Vector<String> messages;
  Suborigin suborigin;
  EXPECT_FALSE(parseSuboriginHeader(header, &suborigin, messages));
  EXPECT_EQ(String(), suborigin.name());
}

TEST(HTTPParsersTest, SuboriginParseValidNames) {
  // Single headers
  expectParseNamePass("Alpha", "foo", "foo");
  expectParseNamePass("Whitespace alpha", "  foo  ", "foo");
  expectParseNamePass("Alphanumeric", "f0o", "f0o");
  expectParseNamePass("Numeric at end", "foo42", "foo42");

  // Mulitple headers should only give the first name
  expectParseNamePass("Multiple headers, no whitespace", "foo,bar", "foo");
  expectParseNamePass("Multiple headers, whitespace before second", "foo, bar",
                      "foo");
  expectParseNamePass(
      "Multiple headers, whitespace after first and before second", "foo, bar",
      "foo");
  expectParseNamePass("Multiple headers, empty second ignored", "foo, bar",
                      "foo");
  expectParseNamePass("Multiple headers, invalid second ignored", "foo, bar",
                      "foo");
}

TEST(HTTPParsersTest, SuboriginParseInvalidNames) {
  // Single header, invalid value
  expectParseNameFail("Empty header", "");
  expectParseNameFail("Numeric", "42");
  expectParseNameFail("Hyphen middle", "foo-bar");
  expectParseNameFail("Hyphen start", "-foobar");
  expectParseNameFail("Hyphen end", "foobar-");
  expectParseNameFail("Whitespace in middle", "foo bar");
  expectParseNameFail("Invalid character at end of name", "foobar'");
  expectParseNameFail("Invalid character at start of name", "'foobar");
  expectParseNameFail("Invalid character in middle of name", "foo'bar");
  expectParseNameFail("Alternate invalid character in middle of name",
                      "foob@r");
  expectParseNameFail("First cap", "Foo");
  expectParseNameFail("All cap", "FOO");

  // Multiple headers, invalid value(s)
  expectParseNameFail("Multple headers, empty first header", ", bar");
  expectParseNameFail("Multple headers, both empty headers", ",");
  expectParseNameFail("Multple headers, invalid character in first header",
                      "f@oo, bar");
  expectParseNameFail("Multple headers, invalid character in both headers",
                      "f@oo, b@r");
}

TEST(HTTPParsersTest, SuboriginParseValidPolicy) {
  const Suborigin::SuboriginPolicyOptions unsafePostmessageSend[] = {
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageSend};
  const Suborigin::SuboriginPolicyOptions unsafePostmessageReceive[] = {
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageReceive};
  const Suborigin::SuboriginPolicyOptions unsafePostmessageSendAndReceive[] = {
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageSend,
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageReceive};
  const Suborigin::SuboriginPolicyOptions unsafeCookies[] = {
      Suborigin::SuboriginPolicyOptions::UnsafeCookies};
  const Suborigin::SuboriginPolicyOptions unsafeCredentials[] = {
      Suborigin::SuboriginPolicyOptions::UnsafeCredentials};
  const Suborigin::SuboriginPolicyOptions allOptions[] = {
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageSend,
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageReceive,
      Suborigin::SuboriginPolicyOptions::UnsafeCookies,
      Suborigin::SuboriginPolicyOptions::UnsafeCredentials};

  // All simple, valid policies
  expectParsePolicyPass(
      "One policy, unsafe-postmessage-send", "foobar 'unsafe-postmessage-send'",
      unsafePostmessageSend, ARRAY_SIZE(unsafePostmessageSend));
  expectParsePolicyPass("One policy, unsafe-postmessage-receive",
                        "foobar 'unsafe-postmessage-receive'",
                        unsafePostmessageReceive,
                        ARRAY_SIZE(unsafePostmessageReceive));
  expectParsePolicyPass("One policy, unsafe-cookies", "foobar 'unsafe-cookies'",
                        unsafeCookies, ARRAY_SIZE(unsafeCookies));
  expectParsePolicyPass("One policy, unsafe-credentials",
                        "foobar 'unsafe-credentials'", unsafeCredentials,
                        ARRAY_SIZE(unsafeCredentials));

  // Formatting differences of policies and multiple policies
  expectParsePolicyPass("One policy, whitespace all around",
                        "foobar      'unsafe-postmessage-send'          ",
                        unsafePostmessageSend,
                        ARRAY_SIZE(unsafePostmessageSend));
  expectParsePolicyPass(
      "Multiple, same policies",
      "foobar 'unsafe-postmessage-send' 'unsafe-postmessage-send'",
      unsafePostmessageSend, ARRAY_SIZE(unsafePostmessageSend));
  expectParsePolicyPass(
      "Multiple, different policies",
      "foobar 'unsafe-postmessage-send' 'unsafe-postmessage-receive'",
      unsafePostmessageSendAndReceive,
      ARRAY_SIZE(unsafePostmessageSendAndReceive));
  expectParsePolicyPass("Many different policies",
                        "foobar 'unsafe-postmessage-send' "
                        "'unsafe-postmessage-receive' 'unsafe-cookies' "
                        "'unsafe-credentials'",
                        allOptions, ARRAY_SIZE(allOptions));
  expectParsePolicyPass("One policy, unknown option", "foobar 'unknown-option'",
                        {}, 0);
}

TEST(HTTPParsersTest, SuboriginParseInvalidPolicy) {
  expectParsePolicyFail("One policy, no suborigin name",
                        "'unsafe-postmessage-send'");
  expectParsePolicyFail("One policy, invalid characters",
                        "foobar 'un$afe-postmessage-send'");
  expectParsePolicyFail("One policy, caps", "foobar 'UNSAFE-POSTMESSAGE-SEND'");
  expectParsePolicyFail("One policy, missing first quote",
                        "foobar unsafe-postmessage-send'");
  expectParsePolicyFail("One policy, missing last quote",
                        "foobar 'unsafe-postmessage-send");
  expectParsePolicyFail("One policy, invalid character at end",
                        "foobar 'unsafe-postmessage-send';");
  expectParsePolicyFail(
      "Multiple policies, extra character between options",
      "foobar 'unsafe-postmessage-send' ; 'unsafe-postmessage-send'");
  expectParsePolicyFail("Policy that is a single quote", "foobar '");
  expectParsePolicyFail("Valid policy and then policy that is a single quote",
                        "foobar 'unsafe-postmessage-send' '");
}

TEST(HTTPParsersTest, ParseHTTPRefresh) {
  double delay;
  String url;
  EXPECT_FALSE(parseHTTPRefresh("", nullptr, delay, url));
  EXPECT_FALSE(parseHTTPRefresh(" ", nullptr, delay, url));

  EXPECT_TRUE(parseHTTPRefresh("123 ", nullptr, delay, url));
  EXPECT_EQ(123.0, delay);
  EXPECT_TRUE(url.isEmpty());

  EXPECT_TRUE(parseHTTPRefresh("1 ; url=dest", nullptr, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(
      parseHTTPRefresh("1 ;\nurl=dest", isASCIISpace<UChar>, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(parseHTTPRefresh("1 ;\nurl=dest", nullptr, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("url=dest", url);

  EXPECT_TRUE(parseHTTPRefresh("1 url=dest", nullptr, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("dest", url);

  EXPECT_TRUE(
      parseHTTPRefresh("10\nurl=dest", isASCIISpace<UChar>, delay, url));
  EXPECT_EQ(10, delay);
  EXPECT_EQ("dest", url);
}

TEST(HTTPParsersTest, ParseMultipartHeadersResult) {
  struct {
    const char* data;
    const bool result;
    const size_t end;
  } tests[] = {
      {"This is junk", false, 0},
      {"Foo: bar\nBaz:\n\nAfter:\n", true, 15},
      {"Foo: bar\nBaz:\n", false, 0},
      {"Foo: bar\r\nBaz:\r\n\r\nAfter:\r\n", true, 18},
      {"Foo: bar\r\nBaz:\r\n", false, 0},
      {"Foo: bar\nBaz:\r\n\r\nAfter:\n\n", true, 17},
      {"Foo: bar\r\nBaz:\n", false, 0},
      {"\r\n", true, 2},
  };
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(tests); ++i) {
    ResourceResponse response;
    size_t end = 0;
    bool result = parseMultipartHeadersFromBody(
        tests[i].data, strlen(tests[i].data), &response, &end);
    EXPECT_EQ(tests[i].result, result);
    EXPECT_EQ(tests[i].end, end);
  }
}

TEST(HTTPParsersTest, ParseMultipartHeaders) {
  ResourceResponse response;
  response.addHTTPHeaderField("foo", "bar");
  response.addHTTPHeaderField("range", "piyo");
  response.addHTTPHeaderField("content-length", "999");

  const char data[] = "content-type: image/png\ncontent-length: 10\n\n";
  size_t end = 0;
  bool result =
      parseMultipartHeadersFromBody(data, strlen(data), &response, &end);

  EXPECT_TRUE(result);
  EXPECT_EQ(strlen(data), end);
  EXPECT_EQ("image/png", response.httpHeaderField("content-type"));
  EXPECT_EQ("10", response.httpHeaderField("content-length"));
  EXPECT_EQ("bar", response.httpHeaderField("foo"));
  EXPECT_EQ(AtomicString(), response.httpHeaderField("range"));
}

TEST(HTTPParsersTest, ParseMultipartHeadersContentCharset) {
  ResourceResponse response;
  const char data[] = "content-type: text/html; charset=utf-8\n\n";
  size_t end = 0;
  bool result =
      parseMultipartHeadersFromBody(data, strlen(data), &response, &end);

  EXPECT_TRUE(result);
  EXPECT_EQ(strlen(data), end);
  EXPECT_EQ("text/html; charset=utf-8",
            response.httpHeaderField("content-type"));
  EXPECT_EQ("utf-8", response.textEncodingName());
}

}  // namespace blink
