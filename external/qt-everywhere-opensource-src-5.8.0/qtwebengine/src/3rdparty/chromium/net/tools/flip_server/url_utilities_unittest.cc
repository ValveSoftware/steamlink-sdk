// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/url_utilities.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(UrlUtilitiesTest, GetUrlHost) {
  EXPECT_EQ("www.foo.com", UrlUtilities::GetUrlHost("http://www.foo.com"));
  EXPECT_EQ("www.foo.com", UrlUtilities::GetUrlHost("http://www.foo.com:80"));
  EXPECT_EQ("www.foo.com", UrlUtilities::GetUrlHost("http://www.foo.com:80/"));
  EXPECT_EQ("www.foo.com", UrlUtilities::GetUrlHost("http://www.foo.com/news"));
  EXPECT_EQ("www.foo.com",
            UrlUtilities::GetUrlHost("www.foo.com:80/news?q=hello"));
  EXPECT_EQ("www.foo.com", UrlUtilities::GetUrlHost("www.foo.com/news?q=a:b"));
  EXPECT_EQ("www.foo.com", UrlUtilities::GetUrlHost("www.foo.com:80"));
}

TEST(UrlUtilitiesTest, GetUrlHostPath) {
  EXPECT_EQ("www.foo.com", UrlUtilities::GetUrlHostPath("http://www.foo.com"));
  EXPECT_EQ("www.foo.com:80",
            UrlUtilities::GetUrlHostPath("http://www.foo.com:80"));
  EXPECT_EQ("www.foo.com:80/",
            UrlUtilities::GetUrlHostPath("http://www.foo.com:80/"));
  EXPECT_EQ("www.foo.com/news",
            UrlUtilities::GetUrlHostPath("http://www.foo.com/news"));
  EXPECT_EQ("www.foo.com:80/news?q=hello",
            UrlUtilities::GetUrlHostPath("www.foo.com:80/news?q=hello"));
  EXPECT_EQ("www.foo.com/news?q=a:b",
            UrlUtilities::GetUrlHostPath("www.foo.com/news?q=a:b"));
  EXPECT_EQ("www.foo.com:80", UrlUtilities::GetUrlHostPath("www.foo.com:80"));
}

TEST(UrlUtilitiesTest, GetUrlPath) {
  EXPECT_EQ("/", UrlUtilities::GetUrlPath("http://www.foo.com"));
  EXPECT_EQ("/", UrlUtilities::GetUrlPath("http://www.foo.com:80"));
  EXPECT_EQ("/", UrlUtilities::GetUrlPath("http://www.foo.com:80/"));
  EXPECT_EQ("/news", UrlUtilities::GetUrlPath("http://www.foo.com/news"));
  EXPECT_EQ("/news?q=hello",
            UrlUtilities::GetUrlPath("www.foo.com:80/news?q=hello"));
  EXPECT_EQ("/news?q=a:b", UrlUtilities::GetUrlPath("www.foo.com/news?q=a:b"));
  EXPECT_EQ("/", UrlUtilities::GetUrlPath("www.foo.com:80"));
}

TEST(UrlUtilitiesTest, Unescape) {
  // Basic examples are left alone.
  EXPECT_EQ("http://www.foo.com", UrlUtilities::Unescape("http://www.foo.com"));
  EXPECT_EQ("www.foo.com:80/news?q=hello",
            UrlUtilities::Unescape("www.foo.com:80/news?q=hello"));

  // All chars can be unescaped.
  EXPECT_EQ("~`!@#$%^&*()_-+={[}]|\\:;\"'<,>.?/",
            UrlUtilities::Unescape("%7E%60%21%40%23%24%25%5E%26%2A%28%29%5F%2D"
                                   "%2B%3D%7B%5B%7D%5D%7C%5C%3A%3B%22%27%3C%2C"
                                   "%3E%2E%3F%2F"));
  for (int c = 0; c < 256; ++c) {
    std::string unescaped_char(1, static_cast<unsigned char>(c));
    std::string escaped_char = base::StringPrintf("%%%02X", c);
    EXPECT_EQ(unescaped_char, UrlUtilities::Unescape(escaped_char))
        << "escaped_char = " << escaped_char;
    escaped_char = base::StringPrintf("%%%02x", c);
    EXPECT_EQ(unescaped_char, UrlUtilities::Unescape(escaped_char))
        << "escaped_char = " << escaped_char;
  }

  // All non-% chars are left alone.
  EXPECT_EQ("~`!@#$^&*()_-+={[}]|\\:;\"'<,>.?/",
            UrlUtilities::Unescape("~`!@#$^&*()_-+={[}]|\\:;\"'<,>.?/"));
  for (int c = 0; c < 256; ++c) {
    if (c != '%') {
      std::string just_char(1, static_cast<unsigned char>(c));
      EXPECT_EQ(just_char, UrlUtilities::Unescape(just_char));
    }
  }

  // Some examples to unescape.
  EXPECT_EQ("Hello, world!", UrlUtilities::Unescape("Hello%2C world%21"));

  // Not actually escapes.
  EXPECT_EQ("%", UrlUtilities::Unescape("%"));
  EXPECT_EQ("%www", UrlUtilities::Unescape("%www"));
  EXPECT_EQ("%foo", UrlUtilities::Unescape("%foo"));
  EXPECT_EQ("%1", UrlUtilities::Unescape("%1"));
  EXPECT_EQ("%1x", UrlUtilities::Unescape("%1x"));
  EXPECT_EQ("%%", UrlUtilities::Unescape("%%"));
  // Escapes following non-escapes.
  EXPECT_EQ("%!", UrlUtilities::Unescape("%%21"));
  EXPECT_EQ("%2!", UrlUtilities::Unescape("%2%21"));
}

}  // namespace net
