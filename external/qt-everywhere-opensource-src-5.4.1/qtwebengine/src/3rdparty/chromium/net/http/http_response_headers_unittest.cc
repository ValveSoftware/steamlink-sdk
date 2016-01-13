// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestData {
  const char* raw_headers;
  const char* expected_headers;
  int expected_response_code;
  net::HttpVersion expected_parsed_version;
  net::HttpVersion expected_version;
};

struct ContentTypeTestData {
  const std::string raw_headers;
  const std::string mime_type;
  const bool has_mimetype;
  const std::string charset;
  const bool has_charset;
  const std::string all_content_type;
};

class HttpResponseHeadersTest : public testing::Test {
};

// Transform "normal"-looking headers (\n-separated) to the appropriate
// input format for ParseRawHeaders (\0-separated).
void HeadersToRaw(std::string* headers) {
  std::replace(headers->begin(), headers->end(), '\n', '\0');
  if (!headers->empty())
    *headers += '\0';
}

void TestCommon(const TestData& test) {
  std::string raw_headers(test.raw_headers);
  HeadersToRaw(&raw_headers);
  std::string expected_headers(test.expected_headers);

  std::string headers;
  scoped_refptr<net::HttpResponseHeaders> parsed(
      new net::HttpResponseHeaders(raw_headers));
  parsed->GetNormalizedHeaders(&headers);

  // Transform to readable output format (so it's easier to see diffs).
  std::replace(headers.begin(), headers.end(), ' ', '_');
  std::replace(headers.begin(), headers.end(), '\n', '\\');
  std::replace(expected_headers.begin(), expected_headers.end(), ' ', '_');
  std::replace(expected_headers.begin(), expected_headers.end(), '\n', '\\');

  EXPECT_EQ(expected_headers, headers);

  EXPECT_EQ(test.expected_response_code, parsed->response_code());

  EXPECT_TRUE(test.expected_parsed_version == parsed->GetParsedHttpVersion());
  EXPECT_TRUE(test.expected_version == parsed->GetHttpVersion());
}

} // end namespace

// Check that we normalize headers properly.
TEST(HttpResponseHeadersTest, NormalizeHeadersWhitespace) {
  TestData test = {
    "HTTP/1.1    202   Accepted  \n"
    "Content-TYPE  : text/html; charset=utf-8  \n"
    "Set-Cookie: a \n"
    "Set-Cookie:   b \n",

    "HTTP/1.1 202 Accepted\n"
    "Content-TYPE: text/html; charset=utf-8\n"
    "Set-Cookie: a, b\n",

    202,
    net::HttpVersion(1,1),
    net::HttpVersion(1,1)
  };
  TestCommon(test);
}

// Check that we normalize headers properly (header name is invalid if starts
// with LWS).
TEST(HttpResponseHeadersTest, NormalizeHeadersLeadingWhitespace) {
  TestData test = {
    "HTTP/1.1    202   Accepted  \n"
    // Starts with space -- will be skipped as invalid.
    "  Content-TYPE  : text/html; charset=utf-8  \n"
    "Set-Cookie: a \n"
    "Set-Cookie:   b \n",

    "HTTP/1.1 202 Accepted\n"
    "Set-Cookie: a, b\n",

    202,
    net::HttpVersion(1,1),
    net::HttpVersion(1,1)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, BlankHeaders) {
  TestData test = {
    "HTTP/1.1 200 OK\n"
    "Header1 :          \n"
    "Header2: \n"
    "Header3:\n"
    "Header4\n"
    "Header5    :\n",

    "HTTP/1.1 200 OK\n"
    "Header1: \n"
    "Header2: \n"
    "Header3: \n"
    "Header5: \n",

    200,
    net::HttpVersion(1,1),
    net::HttpVersion(1,1)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersVersion) {
  // Don't believe the http/0.9 version if there are headers!
  TestData test = {
    "hTtP/0.9 201\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    "HTTP/1.0 201 OK\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    201,
    net::HttpVersion(0,9),
    net::HttpVersion(1,0)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, PreserveHttp09) {
  // Accept the HTTP/0.9 version number if there are no headers.
  // This is how HTTP/0.9 responses get constructed from HttpNetworkTransaction.
  TestData test = {
    "hTtP/0.9 200 OK\n",

    "HTTP/0.9 200 OK\n",

    200,
    net::HttpVersion(0,9),
    net::HttpVersion(0,9)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersMissingOK) {
  TestData test = {
    "HTTP/1.1 201\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    "HTTP/1.1 201 OK\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    201,
    net::HttpVersion(1,1),
    net::HttpVersion(1,1)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersBadStatus) {
  TestData test = {
    "SCREWED_UP_STATUS_LINE\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    "HTTP/1.0 200 OK\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    200,
    net::HttpVersion(0,0), // Parse error
    net::HttpVersion(1,0)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersInvalidStatusCode) {
  TestData test = {
    "HTTP/1.1 -1  Unknown\n",

    "HTTP/1.1 200 OK\n",

    200,
    net::HttpVersion(1,1),
    net::HttpVersion(1,1)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersEmpty) {
  TestData test = {
    "",

    "HTTP/1.0 200 OK\n",

    200,
    net::HttpVersion(0,0), // Parse Error
    net::HttpVersion(1,0)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersStartWithColon) {
  TestData test = {
    "HTTP/1.1    202   Accepted  \n"
    "foo: bar\n"
    ": a \n"
    " : b\n"
    "baz: blat \n",

    "HTTP/1.1 202 Accepted\n"
    "foo: bar\n"
    "baz: blat\n",

    202,
    net::HttpVersion(1,1),
    net::HttpVersion(1,1)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersStartWithColonAtEOL) {
  TestData test = {
    "HTTP/1.1    202   Accepted  \n"
    "foo:   \n"
    "bar:\n"
    "baz: blat \n"
    "zip:\n",

    "HTTP/1.1 202 Accepted\n"
    "foo: \n"
    "bar: \n"
    "baz: blat\n"
    "zip: \n",

    202,
    net::HttpVersion(1,1),
    net::HttpVersion(1,1)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersOfWhitepace) {
  TestData test = {
    "\n   \n",

    "HTTP/1.0 200 OK\n",

    200,
    net::HttpVersion(0,0),  // Parse error
    net::HttpVersion(1,0)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, RepeatedSetCookie) {
  TestData test = {
    "HTTP/1.1 200 OK\n"
    "Set-Cookie: x=1\n"
    "Set-Cookie: y=2\n",

    "HTTP/1.1 200 OK\n"
    "Set-Cookie: x=1, y=2\n",

    200,
    net::HttpVersion(1,1),
    net::HttpVersion(1,1)
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, GetNormalizedHeader) {
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Cache-control: private\n"
      "cache-Control: no-store\n";
  HeadersToRaw(&headers);
  scoped_refptr<net::HttpResponseHeaders> parsed(
      new net::HttpResponseHeaders(headers));

  std::string value;
  EXPECT_TRUE(parsed->GetNormalizedHeader("cache-control", &value));
  EXPECT_EQ("private, no-store", value);
}

TEST(HttpResponseHeadersTest, Persist) {
  const struct {
    net::HttpResponseHeaders::PersistOptions options;
    const char* raw_headers;
    const char* expected_headers;
  } tests[] = {
    { net::HttpResponseHeaders::PERSIST_ALL,
      "HTTP/1.1 200 OK\n"
      "Cache-control:private\n"
      "cache-Control:no-store\n",

      "HTTP/1.1 200 OK\n"
      "Cache-control: private, no-store\n"
    },
    { net::HttpResponseHeaders::PERSIST_SANS_HOP_BY_HOP,
      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "server: blah\n",

      "HTTP/1.1 200 OK\n"
      "server: blah\n"
    },
    { net::HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE |
      net::HttpResponseHeaders::PERSIST_SANS_HOP_BY_HOP,
      "HTTP/1.1 200 OK\n"
      "fOo: 1\n"
      "Foo: 2\n"
      "Transfer-Encoding: chunked\n"
      "CoNnection: keep-alive\n"
      "cache-control: private, no-cache=\"foo\"\n",

      "HTTP/1.1 200 OK\n"
      "cache-control: private, no-cache=\"foo\"\n"
    },
    { net::HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private,no-cache=\"foo, bar\"\n"
      "bar",

      "HTTP/1.1 200 OK\n"
      "Cache-Control: private,no-cache=\"foo, bar\"\n"
    },
    // ignore bogus no-cache value
    { net::HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private,no-cache=foo\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private,no-cache=foo\n"
    },
    // ignore bogus no-cache value
    { net::HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\n"
    },
    // ignore empty no-cache value
    { net::HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\"\"\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\"\"\n"
    },
    // ignore wrong quotes no-cache value
    { net::HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\'foo\'\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\'foo\'\n"
    },
    // ignore unterminated quotes no-cache value
    { net::HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\"foo\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\"foo\n"
    },
    // accept sloppy LWS
    { net::HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\" foo\t, bar\"\n",

      "HTTP/1.1 200 OK\n"
      "Cache-Control: private, no-cache=\" foo\t, bar\"\n"
    },
    // header name appears twice, separated by another header
    { net::HttpResponseHeaders::PERSIST_ALL,
      "HTTP/1.1 200 OK\n"
      "Foo: 1\n"
      "Bar: 2\n"
      "Foo: 3\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 1, 3\n"
      "Bar: 2\n"
    },
    // header name appears twice, separated by another header (type 2)
    { net::HttpResponseHeaders::PERSIST_ALL,
      "HTTP/1.1 200 OK\n"
      "Foo: 1, 3\n"
      "Bar: 2\n"
      "Foo: 4\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 1, 3, 4\n"
      "Bar: 2\n"
    },
    // Test filtering of cookie headers.
    { net::HttpResponseHeaders::PERSIST_SANS_COOKIES,
      "HTTP/1.1 200 OK\n"
      "Set-Cookie: foo=bar; httponly\n"
      "Set-Cookie: bar=foo\n"
      "Bar: 1\n"
      "Set-Cookie2: bar2=foo2\n",

      "HTTP/1.1 200 OK\n"
      "Bar: 1\n"
    },
    // Test LWS at the end of a header.
    { net::HttpResponseHeaders::PERSIST_ALL,
      "HTTP/1.1 200 OK\n"
      "Content-Length: 450   \n"
      "Content-Encoding: gzip\n",

      "HTTP/1.1 200 OK\n"
      "Content-Length: 450\n"
      "Content-Encoding: gzip\n"
    },
    // Test LWS at the end of a header.
    { net::HttpResponseHeaders::PERSIST_RAW,
      "HTTP/1.1 200 OK\n"
      "Content-Length: 450   \n"
      "Content-Encoding: gzip\n",

      "HTTP/1.1 200 OK\n"
      "Content-Length: 450\n"
      "Content-Encoding: gzip\n"
    },
    // Test filtering of transport security state headers.
    { net::HttpResponseHeaders::PERSIST_SANS_SECURITY_STATE,
      "HTTP/1.1 200 OK\n"
      "Strict-Transport-Security: max-age=1576800\n"
      "Bar: 1\n"
      "Public-Key-Pins: max-age=100000; "
          "pin-sha1=\"ObT42aoSpAqWdY9WfRfL7i0HsVk=\";"
          "pin-sha1=\"7kW49EVwZG0hSNx41ZO/fUPN0ek=\"",

      "HTTP/1.1 200 OK\n"
      "Bar: 1\n"
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers = tests[i].raw_headers;
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed1(
        new net::HttpResponseHeaders(headers));

    Pickle pickle;
    parsed1->Persist(&pickle, tests[i].options);

    PickleIterator iter(pickle);
    scoped_refptr<net::HttpResponseHeaders> parsed2(
        new net::HttpResponseHeaders(pickle, &iter));

    std::string h2;
    parsed2->GetNormalizedHeaders(&h2);
    EXPECT_EQ(std::string(tests[i].expected_headers), h2);
  }
}

TEST(HttpResponseHeadersTest, EnumerateHeader_Coalesced) {
  // Ensure that commas in quoted strings are not regarded as value separators.
  // Ensure that whitespace following a value is trimmed properly
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Cache-control:private , no-cache=\"set-cookie,server\" \n"
      "cache-Control: no-store\n";
  HeadersToRaw(&headers);
  scoped_refptr<net::HttpResponseHeaders> parsed(
      new net::HttpResponseHeaders(headers));

  void* iter = NULL;
  std::string value;
  EXPECT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("private", value);
  EXPECT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("no-cache=\"set-cookie,server\"", value);
  EXPECT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("no-store", value);
  EXPECT_FALSE(parsed->EnumerateHeader(&iter, "cache-control", &value));
}

TEST(HttpResponseHeadersTest, EnumerateHeader_Challenge) {
  // Even though WWW-Authenticate has commas, it should not be treated as
  // coalesced values.
  std::string headers =
      "HTTP/1.1 401 OK\n"
      "WWW-Authenticate:Digest realm=foobar, nonce=x, domain=y\n"
      "WWW-Authenticate:Basic realm=quatar\n";
  HeadersToRaw(&headers);
  scoped_refptr<net::HttpResponseHeaders> parsed(
      new net::HttpResponseHeaders(headers));

  void* iter = NULL;
  std::string value;
  EXPECT_TRUE(parsed->EnumerateHeader(&iter, "WWW-Authenticate", &value));
  EXPECT_EQ("Digest realm=foobar, nonce=x, domain=y", value);
  EXPECT_TRUE(parsed->EnumerateHeader(&iter, "WWW-Authenticate", &value));
  EXPECT_EQ("Basic realm=quatar", value);
  EXPECT_FALSE(parsed->EnumerateHeader(&iter, "WWW-Authenticate", &value));
}

TEST(HttpResponseHeadersTest, EnumerateHeader_DateValued) {
  // The comma in a date valued header should not be treated as a
  // field-value separator
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Date: Tue, 07 Aug 2007 23:10:55 GMT\n"
      "Last-Modified: Wed, 01 Aug 2007 23:23:45 GMT\n";
  HeadersToRaw(&headers);
  scoped_refptr<net::HttpResponseHeaders> parsed(
      new net::HttpResponseHeaders(headers));

  std::string value;
  EXPECT_TRUE(parsed->EnumerateHeader(NULL, "date", &value));
  EXPECT_EQ("Tue, 07 Aug 2007 23:10:55 GMT", value);
  EXPECT_TRUE(parsed->EnumerateHeader(NULL, "last-modified", &value));
  EXPECT_EQ("Wed, 01 Aug 2007 23:23:45 GMT", value);
}

TEST(HttpResponseHeadersTest, DefaultDateToGMT) {
  // Verify we make the best interpretation when parsing dates that incorrectly
  // do not end in "GMT" as RFC2616 requires.
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Date: Tue, 07 Aug 2007 23:10:55\n"
      "Last-Modified: Tue, 07 Aug 2007 19:10:55 EDT\n"
      "Expires: Tue, 07 Aug 2007 23:10:55 UTC\n";
  HeadersToRaw(&headers);
  scoped_refptr<net::HttpResponseHeaders> parsed(
      new net::HttpResponseHeaders(headers));
  base::Time expected_value;
  ASSERT_TRUE(base::Time::FromString("Tue, 07 Aug 2007 23:10:55 GMT",
                                     &expected_value));

  base::Time value;
  // When the timezone is missing, GMT is a good guess as its what RFC2616
  // requires.
  EXPECT_TRUE(parsed->GetDateValue(&value));
  EXPECT_EQ(expected_value, value);
  // If GMT is missing but an RFC822-conforming one is present, use that.
  EXPECT_TRUE(parsed->GetLastModifiedValue(&value));
  EXPECT_EQ(expected_value, value);
  // If an unknown timezone is present, treat like a missing timezone and
  // default to GMT.  The only example of a web server not specifying "GMT"
  // used "UTC" which is equivalent to GMT.
  if (parsed->GetExpiresValue(&value))
    EXPECT_EQ(expected_value, value);
}

TEST(HttpResponseHeadersTest, GetMimeType) {
  const ContentTypeTestData tests[] = {
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html\n",
      "text/html", true,
      "", false,
      "text/html" },
    // Multiple content-type headers should give us the last one.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html\n"
      "Content-type: text/html\n",
      "text/html", true,
      "", false,
      "text/html, text/html" },
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/plain\n"
      "Content-type: text/html\n"
      "Content-type: text/plain\n"
      "Content-type: text/html\n",
      "text/html", true,
      "", false,
      "text/plain, text/html, text/plain, text/html" },
    // Test charset parsing.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html\n"
      "Content-type: text/html; charset=ISO-8859-1\n",
      "text/html", true,
      "iso-8859-1", true,
      "text/html, text/html; charset=ISO-8859-1" },
    // Test charset in double quotes.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html\n"
      "Content-type: text/html; charset=\"ISO-8859-1\"\n",
      "text/html", true,
      "iso-8859-1", true,
      "text/html, text/html; charset=\"ISO-8859-1\"" },
    // If there are multiple matching content-type headers, we carry
    // over the charset value.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html;charset=utf-8\n"
      "Content-type: text/html\n",
      "text/html", true,
      "utf-8", true,
      "text/html;charset=utf-8, text/html" },
    // Test single quotes.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html;charset='utf-8'\n"
      "Content-type: text/html\n",
      "text/html", true,
      "utf-8", true,
      "text/html;charset='utf-8', text/html" },
    // Last charset wins if matching content-type.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html;charset=utf-8\n"
      "Content-type: text/html;charset=iso-8859-1\n",
      "text/html", true,
      "iso-8859-1", true,
      "text/html;charset=utf-8, text/html;charset=iso-8859-1" },
    // Charset is ignored if the content types change.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/plain;charset=utf-8\n"
      "Content-type: text/html\n",
      "text/html", true,
      "", false,
      "text/plain;charset=utf-8, text/html" },
    // Empty content-type
    { "HTTP/1.1 200 OK\n"
      "Content-type: \n",
      "", false,
      "", false,
      "" },
    // Emtpy charset
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html;charset=\n",
      "text/html", true,
      "", false,
      "text/html;charset=" },
    // Multiple charsets, last one wins.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html;charset=utf-8; charset=iso-8859-1\n",
      "text/html", true,
      "iso-8859-1", true,
      "text/html;charset=utf-8; charset=iso-8859-1" },
    // Multiple params.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html; foo=utf-8; charset=iso-8859-1\n",
      "text/html", true,
      "iso-8859-1", true,
      "text/html; foo=utf-8; charset=iso-8859-1" },
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html ; charset=utf-8 ; bar=iso-8859-1\n",
      "text/html", true,
      "utf-8", true,
      "text/html ; charset=utf-8 ; bar=iso-8859-1" },
    // Comma embeded in quotes.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html ; charset='utf-8,text/plain' ;\n",
      "text/html", true,
      "utf-8,text/plain", true,
      "text/html ; charset='utf-8,text/plain' ;" },
    // Charset with leading spaces.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html ; charset= 'utf-8' ;\n",
      "text/html", true,
      "utf-8", true,
      "text/html ; charset= 'utf-8' ;" },
    // Media type comments in mime-type.
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html (html)\n",
      "text/html", true,
      "", false,
      "text/html (html)" },
    // Incomplete charset= param
    { "HTTP/1.1 200 OK\n"
      "Content-type: text/html; char=\n",
      "text/html", true,
      "", false,
      "text/html; char=" },
    // Invalid media type: no slash
    { "HTTP/1.1 200 OK\n"
      "Content-type: texthtml\n",
      "", false,
      "", false,
      "texthtml" },
    // Invalid media type: */*
    { "HTTP/1.1 200 OK\n"
      "Content-type: */*\n",
      "", false,
      "", false,
      "*/*" },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string headers(tests[i].raw_headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    std::string value;
    EXPECT_EQ(tests[i].has_mimetype, parsed->GetMimeType(&value));
    EXPECT_EQ(tests[i].mime_type, value);
    value.clear();
    EXPECT_EQ(tests[i].has_charset, parsed->GetCharset(&value));
    EXPECT_EQ(tests[i].charset, value);
    EXPECT_TRUE(parsed->GetNormalizedHeader("content-type", &value));
    EXPECT_EQ(tests[i].all_content_type, value);
  }
}

TEST(HttpResponseHeadersTest, RequiresValidation) {
  const struct {
    const char* headers;
    bool requires_validation;
  } tests[] = {
    // no expiry info: expires immediately
    { "HTTP/1.1 200 OK\n"
      "\n",
      true
    },
    // valid for a little while
    { "HTTP/1.1 200 OK\n"
      "cache-control: max-age=10000\n"
      "\n",
      false
    },
    // expires in the future
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "expires: Wed, 28 Nov 2007 01:00:00 GMT\n"
      "\n",
      false
    },
    // expired already
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "expires: Wed, 28 Nov 2007 00:00:00 GMT\n"
      "\n",
      true
    },
    // max-age trumps expires
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "expires: Wed, 28 Nov 2007 00:00:00 GMT\n"
      "cache-control: max-age=10000\n"
      "\n",
      false
    },
    // last-modified heuristic: modified a while ago
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "last-modified: Wed, 27 Nov 2007 08:00:00 GMT\n"
      "\n",
      false
    },
    { "HTTP/1.1 203 Non-Authoritative Information\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "last-modified: Wed, 27 Nov 2007 08:00:00 GMT\n"
      "\n",
      false
    },
    { "HTTP/1.1 206 Partial Content\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "last-modified: Wed, 27 Nov 2007 08:00:00 GMT\n"
      "\n",
      false
    },
    // last-modified heuristic: modified recently
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "last-modified: Wed, 28 Nov 2007 00:40:10 GMT\n"
      "\n",
      true
    },
    { "HTTP/1.1 203 Non-Authoritative Information\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "last-modified: Wed, 28 Nov 2007 00:40:10 GMT\n"
      "\n",
      true
    },
    { "HTTP/1.1 206 Partial Content\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "last-modified: Wed, 28 Nov 2007 00:40:10 GMT\n"
      "\n",
      true
    },
    // cached permanent redirect
    { "HTTP/1.1 301 Moved Permanently\n"
      "\n",
      false
    },
    // another cached permanent redirect
    { "HTTP/1.1 308 Permanent Redirect\n"
      "\n",
      false
    },
    // cached redirect: not reusable even though by default it would be
    { "HTTP/1.1 300 Multiple Choices\n"
      "Cache-Control: no-cache\n"
      "\n",
      true
    },
    // cached forever by default
    { "HTTP/1.1 410 Gone\n"
      "\n",
      false
    },
    // cached temporary redirect: not reusable
    { "HTTP/1.1 302 Found\n"
      "\n",
      true
    },
    // cached temporary redirect: reusable
    { "HTTP/1.1 302 Found\n"
      "cache-control: max-age=10000\n"
      "\n",
      false
    },
    // cache-control: max-age=N overrides expires: date in the past
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "expires: Wed, 28 Nov 2007 00:20:11 GMT\n"
      "cache-control: max-age=10000\n"
      "\n",
      false
    },
    // cache-control: no-store overrides expires: in the future
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "expires: Wed, 29 Nov 2007 00:40:11 GMT\n"
      "cache-control: no-store,private,no-cache=\"foo\"\n"
      "\n",
      true
    },
    // pragma: no-cache overrides last-modified heuristic
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "last-modified: Wed, 27 Nov 2007 08:00:00 GMT\n"
      "pragma: no-cache\n"
      "\n",
      true
    },
    // TODO(darin): add many many more tests here
  };
  base::Time request_time, response_time, current_time;
  base::Time::FromString("Wed, 28 Nov 2007 00:40:09 GMT", &request_time);
  base::Time::FromString("Wed, 28 Nov 2007 00:40:12 GMT", &response_time);
  base::Time::FromString("Wed, 28 Nov 2007 00:45:20 GMT", &current_time);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    bool requires_validation =
        parsed->RequiresValidation(request_time, response_time, current_time);
    EXPECT_EQ(tests[i].requires_validation, requires_validation);
  }
}

TEST(HttpResponseHeadersTest, Update) {
  const struct {
    const char* orig_headers;
    const char* new_headers;
    const char* expected_headers;
  } tests[] = {
    { "HTTP/1.1 200 OK\n",

      "HTTP/1/1 304 Not Modified\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n",

      "HTTP/1.1 200 OK\n"
      "Cache-control: max-age=10000\n"
    },
    { "HTTP/1.1 200 OK\n"
      "Foo: 1\n"
      "Cache-control: private\n",

      "HTTP/1/1 304 Not Modified\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n",

      "HTTP/1.1 200 OK\n"
      "Cache-control: max-age=10000\n"
      "Foo: 1\n"
    },
    { "HTTP/1.1 200 OK\n"
      "Foo: 1\n"
      "Cache-control: private\n",

      "HTTP/1/1 304 Not Modified\n"
      "connection: keep-alive\n"
      "Cache-CONTROL: max-age=10000\n",

      "HTTP/1.1 200 OK\n"
      "Cache-CONTROL: max-age=10000\n"
      "Foo: 1\n"
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 450\n",

      "HTTP/1/1 304 Not Modified\n"
      "connection: keep-alive\n"
      "Cache-control:      max-age=10001   \n",

      "HTTP/1.1 200 OK\n"
      "Cache-control: max-age=10001\n"
      "Content-Length: 450\n"
    },
    { "HTTP/1.1 200 OK\n"
      "X-Frame-Options: DENY\n",

      "HTTP/1/1 304 Not Modified\n"
      "X-Frame-Options: ALLOW\n",

      "HTTP/1.1 200 OK\n"
      "X-Frame-Options: DENY\n",
    },
    { "HTTP/1.1 200 OK\n"
      "X-WebKit-CSP: default-src 'none'\n",

      "HTTP/1/1 304 Not Modified\n"
      "X-WebKit-CSP: default-src *\n",

      "HTTP/1.1 200 OK\n"
      "X-WebKit-CSP: default-src 'none'\n",
    },
    { "HTTP/1.1 200 OK\n"
      "X-XSS-Protection: 1\n",

      "HTTP/1/1 304 Not Modified\n"
      "X-XSS-Protection: 0\n",

      "HTTP/1.1 200 OK\n"
      "X-XSS-Protection: 1\n",
    },
    { "HTTP/1.1 200 OK\n",

      "HTTP/1/1 304 Not Modified\n"
      "X-Content-Type-Options: nosniff\n",

      "HTTP/1.1 200 OK\n"
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string orig_headers(tests[i].orig_headers);
    HeadersToRaw(&orig_headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(orig_headers));

    std::string new_headers(tests[i].new_headers);
    HeadersToRaw(&new_headers);
    scoped_refptr<net::HttpResponseHeaders> new_parsed(
        new net::HttpResponseHeaders(new_headers));

    parsed->Update(*new_parsed.get());

    std::string resulting_headers;
    parsed->GetNormalizedHeaders(&resulting_headers);
    EXPECT_EQ(std::string(tests[i].expected_headers), resulting_headers);
  }
}

TEST(HttpResponseHeadersTest, EnumerateHeaderLines) {
  const struct {
    const char* headers;
    const char* expected_lines;
  } tests[] = {
    { "HTTP/1.1 200 OK\n",

      ""
    },
    { "HTTP/1.1 200 OK\n"
      "Foo: 1\n",

      "Foo: 1\n"
    },
    { "HTTP/1.1 200 OK\n"
      "Foo: 1\n"
      "Bar: 2\n"
      "Foo: 3\n",

      "Foo: 1\nBar: 2\nFoo: 3\n"
    },
    { "HTTP/1.1 200 OK\n"
      "Foo: 1, 2, 3\n",

      "Foo: 1, 2, 3\n"
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    std::string name, value, lines;

    void* iter = NULL;
    while (parsed->EnumerateHeaderLines(&iter, &name, &value)) {
      lines.append(name);
      lines.append(": ");
      lines.append(value);
      lines.append("\n");
    }

    EXPECT_EQ(std::string(tests[i].expected_lines), lines);
  }
}

TEST(HttpResponseHeadersTest, IsRedirect) {
  const struct {
    const char* headers;
    const char* location;
    bool is_redirect;
  } tests[] = {
    { "HTTP/1.1 200 OK\n",
      "",
      false
    },
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foopy/\n",
      "http://foopy/",
      true
    },
    { "HTTP/1.1 301 Moved\n"
      "Location: \t \n",
      "",
      false
    },
    // we use the first location header as the target of the redirect
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/\n"
      "Location: http://bar/\n",
      "http://foo/",
      true
    },
    // we use the first _valid_ location header as the target of the redirect
    { "HTTP/1.1 301 Moved\n"
      "Location: \n"
      "Location: http://bar/\n",
      "http://bar/",
      true
    },
    // bug 1050541 (location header w/ an unescaped comma)
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/bar,baz.html\n",
      "http://foo/bar,baz.html",
      true
    },
    // bug 1224617 (location header w/ non-ASCII bytes)
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/bar?key=\xE4\xF6\xFC\n",
      "http://foo/bar?key=%E4%F6%FC",
      true
    },
    // Shift_JIS, Big5, and GBK contain multibyte characters with the trailing
    // byte falling in the ASCII range.
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/bar?key=\x81\x5E\xD8\xBF\n",
      "http://foo/bar?key=%81^%D8%BF",
      true
    },
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/bar?key=\x82\x40\xBD\xC4\n",
      "http://foo/bar?key=%82@%BD%C4",
      true
    },
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/bar?key=\x83\x5C\x82\x5D\xCB\xD7\n",
      "http://foo/bar?key=%83\\%82]%CB%D7",
      true
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    std::string location;
    EXPECT_EQ(parsed->IsRedirect(&location), tests[i].is_redirect);
    EXPECT_EQ(location, tests[i].location);
  }
}

TEST(HttpResponseHeadersTest, GetContentLength) {
  const struct {
    const char* headers;
    int64 expected_len;
  } tests[] = {
    { "HTTP/1.1 200 OK\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 10\n",
      10
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: \n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: abc\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: -10\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length:  +10\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 23xb5\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 0xA\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 010\n",
      10
    },
    // Content-Length too big, will overflow an int64
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 40000000000000000000\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length:       10\n",
      10
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 10  \n",
      10
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: \t10\n",
      10
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: \v10\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: \f10\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "cOnTeNt-LENgth: 33\n",
      33
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 34\r\n",
      -1
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    EXPECT_EQ(tests[i].expected_len, parsed->GetContentLength());
  }
}

TEST(HttpResponseHeaders, GetContentRange) {
  const struct {
    const char* headers;
    bool expected_return_value;
    int64 expected_first_byte_position;
    int64 expected_last_byte_position;
    int64 expected_instance_size;
  }  tests[] = {
    { "HTTP/1.1 206 Partial Content",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range:",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: megabytes 0-10/50",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: 0-10/50",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: Bytes 0-50/51",
      true,
      0,
      50,
      51
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0-50/51",
      true,
      0,
      50,
      51
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes\t0-50/51",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range:     bytes 0-50/51",
      true,
      0,
      50,
      51
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range:     bytes    0    -   50  \t / \t51",
      true,
      0,
      50,
      51
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0\t-\t50\t/\t51\t",
      true,
      0,
      50,
      51
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range:   \tbytes\t\t\t 0\t-\t50\t/\t51\t",
      true,
      0,
      50,
      51
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: \t   bytes \t  0    -   50   /   5   1",
      false,
      0,
      50,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: \t   bytes \t  0    -   5 0   /   51",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 50-0/51",
      false,
      50,
      0,
      -1
    },
    { "HTTP/1.1 416 Requested range not satisfiable\n"
      "Content-Range: bytes * /*",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 416 Requested range not satisfiable\n"
      "Content-Range: bytes *   /    *   ",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0-50/*",
      false,
      0,
      50,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0-50  /    * ",
      false,
      0,
      50,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0-10000000000/10000000001",
      true,
      0,
      10000000000ll,
      10000000001ll
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0-10000000000/10000000000",
      false,
      0,
      10000000000ll,
      10000000000ll
    },
    // 64 bits wraparound.
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0 - 9223372036854775807 / 100",
      false,
      0,
      kint64max,
      100
    },
    // 64 bits wraparound.
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0 - 100 / -9223372036854775808",
      false,
      0,
      100,
      kint64min
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes */50",
      false,
      -1,
      -1,
      50
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0-50/10",
      false,
      0,
      50,
      10
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 40-50/45",
      false,
      40,
      50,
      45
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0-50/-10",
      false,
      0,
      50,
      -10
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0-0/1",
      true,
      0,
      0,
      1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0-40000000000000000000/40000000000000000001",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 1-/100",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes -/100",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes -1/100",
      false,
      -1,
      -1,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 0-1233/*",
      false,
      0,
      1233,
      -1
    },
    { "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes -123 - -1/100",
      false,
      -1,
      -1,
      -1
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    int64 first_byte_position;
    int64 last_byte_position;
    int64 instance_size;
    bool return_value = parsed->GetContentRange(&first_byte_position,
                                                &last_byte_position,
                                                &instance_size);
    EXPECT_EQ(tests[i].expected_return_value, return_value);
    EXPECT_EQ(tests[i].expected_first_byte_position, first_byte_position);
    EXPECT_EQ(tests[i].expected_last_byte_position, last_byte_position);
    EXPECT_EQ(tests[i].expected_instance_size, instance_size);
  }
}

TEST(HttpResponseHeadersTest, IsKeepAlive) {
  const struct {
    const char* headers;
    bool expected_keep_alive;
  } tests[] = {
    // The status line fabricated by HttpNetworkTransaction for a 0.9 response.
    // Treated as 0.9.
    { "HTTP/0.9 200 OK",
      false
    },
    // This could come from a broken server.  Treated as 1.0 because it has a
    // header.
    { "HTTP/0.9 200 OK\n"
      "connection: keep-alive\n",
      true
    },
    { "HTTP/1.1 200 OK\n",
      true
    },
    { "HTTP/1.0 200 OK\n",
      false
    },
    { "HTTP/1.0 200 OK\n"
      "connection: close\n",
      false
    },
    { "HTTP/1.0 200 OK\n"
      "connection: keep-alive\n",
      true
    },
    { "HTTP/1.0 200 OK\n"
      "connection: kEeP-AliVe\n",
      true
    },
    { "HTTP/1.0 200 OK\n"
      "connection: keep-aliveX\n",
      false
    },
    { "HTTP/1.1 200 OK\n"
      "connection: close\n",
      false
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n",
      true
    },
    { "HTTP/1.0 200 OK\n"
      "proxy-connection: close\n",
      false
    },
    { "HTTP/1.0 200 OK\n"
      "proxy-connection: keep-alive\n",
      true
    },
    { "HTTP/1.1 200 OK\n"
      "proxy-connection: close\n",
      false
    },
    { "HTTP/1.1 200 OK\n"
      "proxy-connection: keep-alive\n",
      true
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    EXPECT_EQ(tests[i].expected_keep_alive, parsed->IsKeepAlive());
  }
}

TEST(HttpResponseHeadersTest, HasStrongValidators) {
  const struct {
    const char* headers;
    bool expected_result;
  } tests[] = {
    { "HTTP/0.9 200 OK",
      false
    },
    { "HTTP/1.0 200 OK\n"
      "Date: Wed, 28 Nov 2007 01:40:10 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:10 GMT\n"
      "ETag: \"foo\"\n",
      false
    },
    { "HTTP/1.1 200 OK\n"
      "Date: Wed, 28 Nov 2007 01:40:10 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:10 GMT\n"
      "ETag: \"foo\"\n",
      true
    },
    { "HTTP/1.1 200 OK\n"
      "Date: Wed, 28 Nov 2007 00:41:10 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:10 GMT\n",
      true
    },
    { "HTTP/1.1 200 OK\n"
      "Date: Wed, 28 Nov 2007 00:41:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:10 GMT\n",
      false
    },
    { "HTTP/1.1 200 OK\n"
      "ETag: \"foo\"\n",
      true
    },
    // This is not really a weak etag:
    { "HTTP/1.1 200 OK\n"
      "etag: \"w/foo\"\n",
      true
    },
    // This is a weak etag:
    { "HTTP/1.1 200 OK\n"
      "etag: w/\"foo\"\n",
      false
    },
    { "HTTP/1.1 200 OK\n"
      "etag:    W  /   \"foo\"\n",
      false
    }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    EXPECT_EQ(tests[i].expected_result, parsed->HasStrongValidators()) <<
        "Failed test case " << i;
  }
}

TEST(HttpResponseHeadersTest, GetStatusText) {
  std::string headers("HTTP/1.1 404 Not Found");
  HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));
  EXPECT_EQ(std::string("Not Found"), parsed->GetStatusText());
}

TEST(HttpResponseHeadersTest, GetStatusTextMissing) {
  std::string headers("HTTP/1.1 404");
  HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));
  // Since the status line gets normalized, we have OK
  EXPECT_EQ(std::string("OK"), parsed->GetStatusText());
}

TEST(HttpResponseHeadersTest, GetStatusTextMultiSpace) {
  std::string headers("HTTP/1.0     404     Not   Found");
  HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));
  EXPECT_EQ(std::string("Not   Found"), parsed->GetStatusText());
}

TEST(HttpResponseHeadersTest, GetStatusBadStatusLine) {
  std::string headers("Foo bar.");
  HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));
  // The bad status line would have gotten rewritten as
  // HTTP/1.0 200 OK.
  EXPECT_EQ(std::string("OK"), parsed->GetStatusText());
}

TEST(HttpResponseHeadersTest, AddHeader) {
  const struct {
    const char* orig_headers;
    const char* new_header;
    const char* expected_headers;
  } tests[] = {
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n",

      "Content-Length: 450",

      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n"
      "Content-Length: 450\n"
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000    \n",

      "Content-Length: 450  ",

      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n"
      "Content-Length: 450\n"
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string orig_headers(tests[i].orig_headers);
    HeadersToRaw(&orig_headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(orig_headers));

    std::string new_header(tests[i].new_header);
    parsed->AddHeader(new_header);

    std::string resulting_headers;
    parsed->GetNormalizedHeaders(&resulting_headers);
    EXPECT_EQ(std::string(tests[i].expected_headers), resulting_headers);
  }
}

TEST(HttpResponseHeadersTest, RemoveHeader) {
  const struct {
    const char* orig_headers;
    const char* to_remove;
    const char* expected_headers;
  } tests[] = {
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n"
      "Content-Length: 450\n",

      "Content-Length",

      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n"
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive  \n"
      "Content-Length  : 450  \n"
      "Cache-control: max-age=10000\n",

      "Content-Length",

      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n"
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string orig_headers(tests[i].orig_headers);
    HeadersToRaw(&orig_headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(orig_headers));

    std::string name(tests[i].to_remove);
    parsed->RemoveHeader(name);

    std::string resulting_headers;
    parsed->GetNormalizedHeaders(&resulting_headers);
    EXPECT_EQ(std::string(tests[i].expected_headers), resulting_headers);
  }
}

TEST(HttpResponseHeadersTest, RemoveIndividualHeader) {
  const struct {
    const char* orig_headers;
    const char* to_remove_name;
    const char* to_remove_value;
    const char* expected_headers;
  } tests[] = {
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n"
      "Content-Length: 450\n",

      "Content-Length",

      "450",

      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n"
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive  \n"
      "Content-Length  : 450  \n"
      "Cache-control: max-age=10000\n",

      "Content-Length",

      "450",

      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n"
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive  \n"
      "Content-Length: 450\n"
      "Cache-control: max-age=10000\n",

      "Content-Length",  // Matching name.

      "999",  // Mismatching value.

      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Content-Length: 450\n"
      "Cache-control: max-age=10000\n"
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive  \n"
      "Foo: bar, baz\n"
      "Foo: bar\n"
      "Cache-control: max-age=10000\n",

      "Foo",

      "bar, baz",  // Space in value.

      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Foo: bar\n"
      "Cache-control: max-age=10000\n"
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive  \n"
      "Foo: bar, baz\n"
      "Cache-control: max-age=10000\n",

      "Foo",

      "baz",  // Only partial match -> ignored.

      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Foo: bar, baz\n"
      "Cache-control: max-age=10000\n"
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string orig_headers(tests[i].orig_headers);
    HeadersToRaw(&orig_headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(orig_headers));

    std::string name(tests[i].to_remove_name);
    std::string value(tests[i].to_remove_value);
    parsed->RemoveHeaderLine(name, value);

    std::string resulting_headers;
    parsed->GetNormalizedHeaders(&resulting_headers);
    EXPECT_EQ(std::string(tests[i].expected_headers), resulting_headers);
  }
}

TEST(HttpResponseHeadersTest, ReplaceStatus) {
  const struct {
    const char* orig_headers;
    const char* new_status;
    const char* expected_headers;
  } tests[] = {
    { "HTTP/1.1 206 Partial Content\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n"
      "Content-Length: 450\n",

      "HTTP/1.1 200 OK",

      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n"
      "Content-Length: 450\n"
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n",

      "HTTP/1.1 304 Not Modified",

      "HTTP/1.1 304 Not Modified\n"
      "connection: keep-alive\n"
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive  \n"
      "Content-Length  : 450   \n"
      "Cache-control: max-age=10000\n",

      "HTTP/1//1 304 Not Modified",

      "HTTP/1.0 304 Not Modified\n"
      "connection: keep-alive\n"
      "Content-Length: 450\n"
      "Cache-control: max-age=10000\n"
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string orig_headers(tests[i].orig_headers);
    HeadersToRaw(&orig_headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(orig_headers));

    std::string name(tests[i].new_status);
    parsed->ReplaceStatusLine(name);

    std::string resulting_headers;
    parsed->GetNormalizedHeaders(&resulting_headers);
    EXPECT_EQ(std::string(tests[i].expected_headers), resulting_headers);
  }
}

TEST(HttpResponseHeadersTest, UpdateWithNewRange) {
  const struct {
    const char* orig_headers;
    const char* expected_headers;
    const char* expected_headers_with_replaced_status;
  } tests[] = {
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 450\n",

      "HTTP/1.1 200 OK\n"
      "Content-Range: bytes 3-5/450\n"
      "Content-Length: 3\n",

      "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 3-5/450\n"
      "Content-Length: 3\n",
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 5\n",

      "HTTP/1.1 200 OK\n"
      "Content-Range: bytes 3-5/5\n"
      "Content-Length: 3\n",

      "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 3-5/5\n"
      "Content-Length: 3\n",
    },
  };
  const net::HttpByteRange range = net::HttpByteRange::Bounded(3, 5);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string orig_headers(tests[i].orig_headers);
    std::replace(orig_headers.begin(), orig_headers.end(), '\n', '\0');
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(orig_headers + '\0'));
    int64 content_size = parsed->GetContentLength();
    std::string resulting_headers;

    // Update headers without replacing status line.
    parsed->UpdateWithNewRange(range, content_size, false);
    parsed->GetNormalizedHeaders(&resulting_headers);
    EXPECT_EQ(std::string(tests[i].expected_headers), resulting_headers);

    // Replace status line too.
    parsed->UpdateWithNewRange(range, content_size, true);
    parsed->GetNormalizedHeaders(&resulting_headers);
    EXPECT_EQ(std::string(tests[i].expected_headers_with_replaced_status),
              resulting_headers);
  }
}

TEST(HttpResponseHeadersTest, ToNetLogParamAndBackAgain) {
  std::string headers("HTTP/1.1 404\n"
                      "Content-Length: 450\n"
                      "Connection: keep-alive\n");
  HeadersToRaw(&headers);
  scoped_refptr<net::HttpResponseHeaders> parsed(
      new net::HttpResponseHeaders(headers));

  scoped_ptr<base::Value> event_param(
      parsed->NetLogCallback(net::NetLog::LOG_ALL_BUT_BYTES));
  scoped_refptr<net::HttpResponseHeaders> recreated;

  ASSERT_TRUE(net::HttpResponseHeaders::FromNetLogParam(event_param.get(),
                                                        &recreated));
  ASSERT_TRUE(recreated.get());
  EXPECT_EQ(parsed->GetHttpVersion(), recreated->GetHttpVersion());
  EXPECT_EQ(parsed->response_code(), recreated->response_code());
  EXPECT_EQ(parsed->GetContentLength(), recreated->GetContentLength());
  EXPECT_EQ(parsed->IsKeepAlive(), recreated->IsKeepAlive());

  std::string normalized_parsed;
  parsed->GetNormalizedHeaders(&normalized_parsed);
  std::string normalized_recreated;
  parsed->GetNormalizedHeaders(&normalized_recreated);
  EXPECT_EQ(normalized_parsed, normalized_recreated);
}
