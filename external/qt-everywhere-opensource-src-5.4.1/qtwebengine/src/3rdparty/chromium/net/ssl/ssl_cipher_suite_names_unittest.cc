// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_cipher_suite_names.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(CipherSuiteNamesTest, Basic) {
  const char *key_exchange, *cipher, *mac;
  bool is_aead;

  SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead, 0xc001);
  EXPECT_STREQ("ECDH_ECDSA", key_exchange);
  EXPECT_STREQ("NULL", cipher);
  EXPECT_STREQ("SHA1", mac);
  EXPECT_FALSE(is_aead);

  SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead, 0x009f);
  EXPECT_STREQ("DHE_RSA", key_exchange);
  EXPECT_STREQ("AES_256_GCM", cipher);
  EXPECT_TRUE(is_aead);
  EXPECT_EQ(NULL, mac);

  SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead, 0xff31);
  EXPECT_STREQ("???", key_exchange);
  EXPECT_STREQ("???", cipher);
  EXPECT_STREQ("???", mac);
  EXPECT_FALSE(is_aead);
}

TEST(CipherSuiteNamesTest, ParseSSLCipherString) {
  uint16 cipher_suite = 0;
  EXPECT_TRUE(ParseSSLCipherString("0x0004", &cipher_suite));
  EXPECT_EQ(0x00004u, cipher_suite);

  EXPECT_TRUE(ParseSSLCipherString("0xBEEF", &cipher_suite));
  EXPECT_EQ(0xBEEFu, cipher_suite);
}

TEST(CipherSuiteNamesTest, ParseSSLCipherStringFails) {
  const char* const cipher_strings[] = {
    "0004",
    "0x004",
    "0xBEEFY",
  };

  for (size_t i = 0; i < arraysize(cipher_strings); ++i) {
    uint16 cipher_suite = 0;
    EXPECT_FALSE(ParseSSLCipherString(cipher_strings[i], &cipher_suite));
  }
}

TEST(CipherSuiteNamesTest, SecureCipherSuites) {
  // Picked some random cipher suites.
  EXPECT_FALSE(IsSecureTLSCipherSuite(0x0));
  EXPECT_FALSE(IsSecureTLSCipherSuite(0x39));
  EXPECT_FALSE(IsSecureTLSCipherSuite(0xc5));
  EXPECT_FALSE(IsSecureTLSCipherSuite(0xc00f));
  EXPECT_FALSE(IsSecureTLSCipherSuite(0xc083));

  // Non-existent cipher suite.
  EXPECT_FALSE(IsSecureTLSCipherSuite(0xffff)) << "Doesn't exist!";

  // Secure ones.
  EXPECT_TRUE(IsSecureTLSCipherSuite(0xcc13));
  EXPECT_TRUE(IsSecureTLSCipherSuite(0xcc14));
}

}  // anonymous namespace

}  // namespace net
