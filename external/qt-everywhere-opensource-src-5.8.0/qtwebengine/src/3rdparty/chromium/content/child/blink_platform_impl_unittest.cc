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

TEST(BlinkPlatformTest, castWebSecurityOrigin) {
  struct TestCase {
    const char* origin;
    const char* scheme;
    const char* host;
    uint16_t port;
  } cases[] = {
      {"http://example.com", "http", "example.com", 80},
      {"http://example.com:80", "http", "example.com", 80},
      {"http://example.com:81", "http", "example.com", 81},
      {"https://example.com", "https", "example.com", 443},
      {"https://example.com:443", "https", "example.com", 443},
      {"https://example.com:444", "https", "example.com", 444},
  };

  for (const auto& test : cases) {
    blink::WebSecurityOrigin web_origin =
        blink::WebSecurityOrigin::createFromString(
            blink::WebString::fromUTF8(test.origin));
    EXPECT_EQ(test.scheme, web_origin.protocol().utf8());
    EXPECT_EQ(test.host, web_origin.host().utf8());
    EXPECT_EQ(test.port, web_origin.effectivePort());

    url::Origin url_origin = web_origin;
    EXPECT_EQ(test.scheme, url_origin.scheme());
    EXPECT_EQ(test.host, url_origin.host());
    EXPECT_EQ(test.port, url_origin.port());

    web_origin = url::Origin(GURL(test.origin));
    EXPECT_EQ(test.scheme, web_origin.protocol().utf8());
    EXPECT_EQ(test.host, web_origin.host().utf8());
    EXPECT_EQ(test.port, web_origin.effectivePort());
  }

  blink::WebSecurityOrigin web_origin =
      blink::WebSecurityOrigin::createUnique();
  EXPECT_TRUE(web_origin.isUnique());

  url::Origin url_origin = web_origin;
  EXPECT_TRUE(url_origin.unique());

  web_origin = url::Origin(GURL(""));
  EXPECT_TRUE(web_origin.isUnique());
}

}  // namespace content
