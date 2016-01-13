// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <vector>

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/random.h"
#include "crypto/secure_util.h"
#include "google_apis/cup/client_update_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string GetPublicKeyForTesting() {
  // How to generate this key:
  //   openssl genpkey -out cr.pem -outform PEM -algorithm RSA
  //                   -pkeyopt rsa_keygen_pubexp:3
  //   openssl rsa -in cr.pem -pubout -out cr_pub.pem

  static const char kCupTestKey1024_Base64[] =
    "MIGdMA0GCSqGSIb3DQEBAQUAA4GLADCBhwKBgQC7ct1JhLSol2DkBcJdNjR3KkEA"
    "ZfXpF22lDD2WZu5JAZ4NiZqnHsKGJNPUbCH4AhFsXmuW5wEHhUVNhsMP6F9mQ06D"
    "i+ygwZ8aXlklmW4S0Et+SNg3i73fnYn0KDQzrzJnMu46s/CFPhjr4f0TH9b7oHkU"
    "XbqNZtG6gwaN1bmzFwIBAw==";

  std::string result;
  if (!base::Base64Decode(std::string(kCupTestKey1024_Base64), &result))
    return "";

  return result;
}

}  // end namespace

#if defined(USE_OPENSSL)

// Once CUP is implemented for OpenSSL, remove this #if block.
TEST(CupTest, OpenSSLStub) {
  scoped_ptr<ClientUpdateProtocol> cup =
      ClientUpdateProtocol::Create(8, GetPublicKeyForTesting());
  ASSERT_FALSE(cup.get());
}

#else

class CupTest : public testing::Test {
 protected:
  virtual void SetUp() {
    cup_ = ClientUpdateProtocol::Create(8, GetPublicKeyForTesting());
    ASSERT_TRUE(cup_.get());
  }

  void OverrideRAndRebuildKeys() {
    // This must be the same length as the modulus of the public key being
    // used in the unit test.
    static const size_t kPublicKeyLength = 1024 / 8;
    static const char kFixedR[] =
      "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do "
      "eiusmod tempor incididunt ut labore et dolore magna aliqua.    ";

    ASSERT_EQ(kPublicKeyLength, strlen(kFixedR));
    ASSERT_TRUE(cup_->SetSharedKeyForTesting(kFixedR));
  }

  ClientUpdateProtocol& CUP() {
    return *cup_.get();
  }

 private:
  scoped_ptr<ClientUpdateProtocol> cup_;
};

TEST_F(CupTest, GetVersionedSecret) {
  // Given a fixed public key set in the test fixture, if the key source |r|
  // is filled with known data, |w| can be tested against an expected output,
  // and the signing/verification for an given request becomes fixed.
  //
  // The expected output can be generated using this command line, where
  // plaintext.bin is the contents of kFixedR[] in OverrideRAndRebuildKeys():
  //
  //   openssl rsautl -inkey cr2_pub.pem -pubin -encrypt -raw
  //                  -in plaintext.bin | base64
  //
  // Remember to prepend the key version number, and fix up the Base64
  // afterwards to be URL-safe.

  static const char kExpectedVW[] =
    "8:lMmNR3mVbOitbq8ceYGStFBwrJcpvY-sauFSbMVe6VONS9x42xTOLY_KdqsWCy"
    "KuiJBiQziQLOybPUyA9vk0N5kMnC90LIh2nP2FgFG0M0Z22qjB3drsdJPi7TQZbb"
    "Xhqm587M8vjc6VlM_eoC0qYwCPaXBqXjsyiHnXetcn5X0";

  EXPECT_NE(kExpectedVW, CUP().GetVersionedSecret());
  OverrideRAndRebuildKeys();
  EXPECT_EQ(kExpectedVW, CUP().GetVersionedSecret());
}

TEST_F(CupTest, SignRequest) {
  static const char kUrl[] = "//testserver.chromium.org/update";
  static const char kUrlQuery[] = "?junk=present";
  static const char kRequest[] = "testbody";

  static const char kExpectedCP[] = "tfjmVMDAbU0-Kye4PjrCuyQIDCU";

  OverrideRAndRebuildKeys();

  // Check the case with no query string other than v|w.
  std::string url(kUrl);
  url.append("?w=");
  url.append(CUP().GetVersionedSecret());

  std::string cp;
  ASSERT_TRUE(CUP().SignRequest(url, kRequest, &cp));

  // Check the case with a pre-existing query string.
  std::string url2(kUrl);
  url2.append(kUrlQuery);
  url2.append("&w=");
  url2.append(CUP().GetVersionedSecret());

  std::string cp2;
  ASSERT_TRUE(CUP().SignRequest(url2, kRequest, &cp2));

  // Changes in the URL should result in changes in the client proof.
  EXPECT_EQ(kExpectedCP, cp2);
  EXPECT_NE(cp, cp2);
}

TEST_F(CupTest, ValidateResponse) {
  static const char kUrl[] = "//testserver.chromium.orgupdate?junk=present&w=";
  static const char kRequest[] = "testbody";

  static const char kGoodResponse[] = "intact_response";
  static const char kGoodC[] = "c=EncryptedDataFromTheUpdateServer";
  static const char kGoodSP[] = "5rMFMPL9Hgqb-2J8kL3scsHeNgg";

  static const char kBadResponse[] = "tampered_response";
  static const char kBadC[] = "c=TotalJunkThatAnAttackerCouldSend";
  static const char kBadSP[] = "Base64TamperedShaOneHash";

  OverrideRAndRebuildKeys();

  std::string url(kUrl);
  url.append(CUP().GetVersionedSecret());

  std::string client_proof;
  ASSERT_TRUE(CUP().SignRequest(url, kRequest, &client_proof));

  // Return true on a valid response and server proof.
  EXPECT_TRUE(CUP().ValidateResponse(kGoodResponse, kGoodC, kGoodSP));

  // Return false on anything invalid.
  EXPECT_FALSE(CUP().ValidateResponse(kBadResponse, kGoodC, kGoodSP));
  EXPECT_FALSE(CUP().ValidateResponse(kGoodResponse, kBadC, kGoodSP));
  EXPECT_FALSE(CUP().ValidateResponse(kGoodResponse, kGoodC, kBadSP));
  EXPECT_FALSE(CUP().ValidateResponse(kGoodResponse, kBadC, kBadSP));
  EXPECT_FALSE(CUP().ValidateResponse(kBadResponse, kGoodC, kBadSP));
  EXPECT_FALSE(CUP().ValidateResponse(kBadResponse, kBadC, kGoodSP));
  EXPECT_FALSE(CUP().ValidateResponse(kBadResponse, kBadC, kBadSP));
}

#endif  // !defined(USE_OPENSSL)

