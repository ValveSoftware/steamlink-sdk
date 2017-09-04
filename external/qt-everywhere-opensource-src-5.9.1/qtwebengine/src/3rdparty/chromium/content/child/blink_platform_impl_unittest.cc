// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blink_platform_impl.h"

#include <stdint.h>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "url/origin.h"

namespace content {

void CheckCastedOriginsAlreadyNormalized(
    const blink::WebSecurityOrigin& origin) {
  url::Origin checked_origin =
      url::Origin::UnsafelyCreateOriginWithoutNormalization(
          origin.protocol().utf8(), origin.host().utf8(),
          origin.effectivePort());
  url::Origin non_checked_origin = url::Origin::CreateFromNormalizedTuple(
      origin.protocol().utf8(), origin.host().utf8(), origin.effectivePort());
  EXPECT_EQ(checked_origin.scheme(), non_checked_origin.scheme());
  EXPECT_EQ(checked_origin.host(), non_checked_origin.host());
  EXPECT_EQ(checked_origin.port(), non_checked_origin.port());
}

TEST(BlinkPlatformTest, CastWebSecurityOrigin) {
  struct TestCase {
    const char* url;
    const char* scheme;
    const char* host;
    uint16_t port;
    const char* suborigin;
  } cases[] = {
      {"http://example.com", "http", "example.com", 80, ""},
      {"http://example.com:80", "http", "example.com", 80, ""},
      {"http://example.com:81", "http", "example.com", 81, ""},
      {"https://example.com", "https", "example.com", 443, ""},
      {"https://example.com:443", "https", "example.com", 443, ""},
      {"https://example.com:444", "https", "example.com", 444, ""},

      // Copied from url/origin_unittest.cc

      // IP Addresses
      {"http://192.168.9.1/", "http", "192.168.9.1", 80, ""},
      {"http://[2001:db8::1]/", "http", "[2001:db8::1]", 80, ""},

      // Punycode
      {"http://☃.net/", "http", "xn--n3h.net", 80, ""},
      {"blob:http://☃.net/", "http", "xn--n3h.net", 80, ""},

      // Generic URLs
      {"http://example.com/", "http", "example.com", 80, ""},
      {"http://example.com:123/", "http", "example.com", 123, ""},
      {"https://example.com/", "https", "example.com", 443, ""},
      {"https://example.com:123/", "https", "example.com", 123, ""},
      {"http://user:pass@example.com/", "http", "example.com", 80, ""},
      {"http://example.com:123/?query", "http", "example.com", 123, ""},
      {"https://example.com/#1234", "https", "example.com", 443, ""},
      {"https://u:p@example.com:123/?query#1234", "https", "example.com", 123,
       ""},

      // Registered URLs
      {"ftp://example.com/", "ftp", "example.com", 21, ""},
      {"ws://example.com/", "ws", "example.com", 80, ""},
      {"wss://example.com/", "wss", "example.com", 443, ""},

      // file: URLs
      {"file:///etc/passwd", "file", "", 0, ""},
      {"file://example.com/etc/passwd", "file", "example.com", 0, ""},

      // Filesystem:
      {"filesystem:http://example.com/type/", "http", "example.com", 80, ""},
      {"filesystem:http://example.com:123/type/", "http", "example.com", 123,
       ""},
      {"filesystem:https://example.com/type/", "https", "example.com", 443, ""},
      {"filesystem:https://example.com:123/type/", "https", "example.com", 123,
       ""},

      // Blob:
      {"blob:http://example.com/guid-goes-here", "http", "example.com", 80, ""},
      {"blob:http://example.com:123/guid-goes-here", "http", "example.com", 123,
       ""},
      {"blob:https://example.com/guid-goes-here", "https", "example.com", 443,
       ""},
      {"blob:http://u:p@example.com/guid-goes-here", "http", "example.com", 80,
       ""},
      {"blob:https://example.co.uk/guid-goes-here", "https", "example.co.uk",
       443, ""},

      // Suborigins
      {"http-so://foobar.example.com", "http", "example.com", 80, "foobar"},
      {"http-so://foobar.example.com:80", "http", "example.com", 80, "foobar"},
      {"http-so://foobar.example.com:81", "http", "example.com", 81, "foobar"},
      {"https-so://foobar.example.com", "https", "example.com", 443, "foobar"},
      {"https-so://foobar.example.com:443", "https", "example.com", 443,
       "foobar"},
      {"https-so://foobar.example.com:444", "https", "example.com", 444,
       "foobar"},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << test.url);
    blink::WebSecurityOrigin web_origin =
        blink::WebSecurityOrigin::createFromString(
            blink::WebString::fromUTF8(test.url));
    EXPECT_EQ(test.scheme, web_origin.protocol().utf8());
    EXPECT_EQ(test.host, web_origin.host().utf8());
    EXPECT_EQ(test.port, web_origin.effectivePort());
    EXPECT_EQ(test.suborigin, web_origin.suborigin().utf8());

    url::Origin url_origin = web_origin;
    EXPECT_EQ(test.scheme, url_origin.scheme());
    EXPECT_EQ(test.host, url_origin.host());
    EXPECT_EQ(test.port, url_origin.port());
    EXPECT_EQ(test.suborigin, url_origin.suborigin());

    web_origin = url::Origin(GURL(test.url));
    EXPECT_EQ(test.scheme, web_origin.protocol().utf8());
    EXPECT_EQ(test.host, web_origin.host().utf8());
    EXPECT_EQ(test.port, web_origin.effectivePort());
    EXPECT_EQ(test.suborigin, web_origin.suborigin().utf8());

    CheckCastedOriginsAlreadyNormalized(web_origin);
  }

  {
    SCOPED_TRACE(testing::Message() << "null");
    blink::WebSecurityOrigin web_origin =
        blink::WebSecurityOrigin::createUnique();
    EXPECT_TRUE(web_origin.isUnique());

    url::Origin url_origin = web_origin;
    EXPECT_TRUE(url_origin.unique());
    EXPECT_EQ("", url_origin.suborigin());

    web_origin = url::Origin(GURL(""));
    EXPECT_TRUE(web_origin.isUnique());
  }
}

// This test ensures that WebSecurityOrigins can safely use
// url::Origin::CreateFromNormalizedTuple when doing conversions.
TEST(BlinkPlatformTest, WebSecurityOriginNormalization) {
  struct TestCases {
    const char* url;
  } cases[] = {{""},
               {"javascript:alert(1)"},
               {"file://example.com:443/etc/passwd"},
               {"blob:https://example.com/uuid-goes-here"},
               {"filesystem:https://example.com/temporary/yay.png"},
               {"data"},
               {"blob:"},
               {"chrome://,/"},
               {"xkcd://927"},
               {"filesystem"},
               {"data://example.com:80"},
               {"http://☃.net:80"},
               {"http\nmore://example.com:80"},
               {"http\rmore://:example.com:80"},
               {"http\n://example.com:80"},
               {"http\r://example.com:80"},
               {"http://example.com\nnot-example.com:80"},
               {"http://example.com\rnot-example.com:80"},
               {"http://example.com\n:80"},
               {"http://example.com\r:80"},
               {"http://example.com:0"},
               {"http://EXAMPLE.com"},
               {"http://EXAMPLE.com/%3Afoo"},
               {"https://example.com:443"},
               {"file:///"},
               {"file:///root:80"}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << test.url);
    blink::WebSecurityOrigin web_origin =
        blink::WebSecurityOrigin::createFromString(
            blink::WebString::fromUTF8(test.url));
    CheckCastedOriginsAlreadyNormalized(web_origin);
  }
}

}  // namespace content
