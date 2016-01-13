// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_log_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HttpLogUtilTest, ElideHeaderValueForNetLog) {
  // Only elide for appropriate log level.
  EXPECT_EQ("[10 bytes were stripped]", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA, "Cookie", "name=value"));
  EXPECT_EQ("name=value", ElideHeaderValueForNetLog(
      net::NetLog::LOG_ALL_BUT_BYTES, "Cookie", "name=value"));

  // Headers are compared case insensitively.
  EXPECT_EQ("[10 bytes were stripped]", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA, "cOoKiE", "name=value"));

  // These headers should be completely elided.
  EXPECT_EQ("[10 bytes were stripped]", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA, "Set-Cookie", "name=value"));
  EXPECT_EQ("[10 bytes were stripped]", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA, "Set-Cookie2", "name=value"));
  EXPECT_EQ("[10 bytes were stripped]", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA, "Authorization", "Basic 1234"));
#if !defined(SPDY_PROXY_AUTH_ORIGIN)
  EXPECT_EQ("[10 bytes were stripped]", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA,
      "Proxy-Authorization", "Basic 1234"));
#endif

  // Unknown headers should pass through.
  EXPECT_EQ("value", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA, "Boring", "value"));

  // Basic and Digest auth challenges are public.
  EXPECT_EQ("Basic realm=test", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA,
      "WWW-Authenticate", "Basic realm=test"));
  EXPECT_EQ("Digest realm=test", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA,
      "WWW-Authenticate", "Digest realm=test"));
#if !defined(SPDY_PROXY_AUTH_ORIGIN)
  EXPECT_EQ("Basic realm=test", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA,
      "Proxy-Authenticate", "Basic realm=test"));
  EXPECT_EQ("Digest realm=test", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA,
      "Proxy-Authenticate", "Digest realm=test"));
#endif

  // Multi-round mechanisms partially elided.
  EXPECT_EQ("NTLM [4 bytes were stripped]", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA, "WWW-Authenticate", "NTLM 1234"));
#if !defined(SPDY_PROXY_AUTH_ORIGIN)
  EXPECT_EQ("NTLM [4 bytes were stripped]", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA, "Proxy-Authenticate", "NTLM 1234"));
#endif

  // Leave whitespace intact.
  EXPECT_EQ("NTLM  [4 bytes were stripped] ", ElideHeaderValueForNetLog(
      net::NetLog::LOG_STRIP_PRIVATE_DATA, "WWW-Authenticate", "NTLM  1234 "));

  // Extra elisions for SPDY_PROXY_AUTH_ORIGIN.
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  EXPECT_EQ("[elided]", ElideHeaderValueForNetLog(
      net::NetLog::LOG_ALL_BUT_BYTES,
      "Proxy-Authenticate", "Basic realm=test"));
  EXPECT_EQ("[elided]", ElideHeaderValueForNetLog(
      net::NetLog::LOG_ALL_BUT_BYTES, "Proxy-Authorization", "Basic 1234"));
#endif
}

}  // namspace net
