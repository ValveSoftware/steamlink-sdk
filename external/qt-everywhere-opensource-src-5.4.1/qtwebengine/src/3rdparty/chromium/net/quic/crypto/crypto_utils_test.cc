// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

TEST(CryptoUtilsTest, IsValidSNI) {
  // IP as SNI.
  EXPECT_FALSE(CryptoUtils::IsValidSNI("192.168.0.1"));
  // SNI without any dot.
  EXPECT_FALSE(CryptoUtils::IsValidSNI("somedomain"));
  // Invalid RFC2396 hostname
  // TODO(rtenneti): Support RFC2396 hostname.
  // EXPECT_FALSE(CryptoUtils::IsValidSNI("some_domain.com"));
  // An empty string must be invalid otherwise the QUIC client will try sending
  // it.
  EXPECT_FALSE(CryptoUtils::IsValidSNI(""));

  // Valid SNI
  EXPECT_TRUE(CryptoUtils::IsValidSNI("test.google.com"));
}

TEST(CryptoUtilsTest, NormalizeHostname) {
  struct {
    const char *input, *expected;
  } tests[] = {
    { "www.google.com", "www.google.com", },
    { "WWW.GOOGLE.COM", "www.google.com", },
    { "www.google.com.", "www.google.com", },
    { "www.google.COM.", "www.google.com", },
    { "www.google.com..", "www.google.com", },
    { "www.google.com........", "www.google.com", },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    EXPECT_EQ(std::string(tests[i].expected),
              CryptoUtils::NormalizeHostname(tests[i].input));
  }
}

}  // namespace
}  // namespace test
}  // namespace net
