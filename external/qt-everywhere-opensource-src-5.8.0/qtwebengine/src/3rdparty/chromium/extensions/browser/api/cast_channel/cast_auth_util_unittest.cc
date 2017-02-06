// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_util.h"

#include <string>

#include "base/macros.h"
#include "components/cast_certificate/cast_cert_validator_test_helpers.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace api {
namespace cast_channel {
namespace {

class CastAuthUtilTest : public testing::Test {
 public:
  CastAuthUtilTest() {}
  ~CastAuthUtilTest() override {}

  void SetUp() override {}

 protected:
  static AuthResponse CreateAuthResponse(std::string* signed_data) {
    auto chain = cast_certificate::testing::ReadCertificateChainFromFile(
        "certificates/chromecast_gen1.pem");
    CHECK(!chain.empty());

    auto signature_data = cast_certificate::testing::ReadSignatureTestData(
        "signeddata/2ZZBG9_FA8FCA3EF91A.pem");

    AuthResponse response;

    response.set_client_auth_certificate(chain[0]);
    for (size_t i = 1; i < chain.size(); ++i)
      response.add_intermediate_certificate(chain[i]);

    response.set_signature(signature_data.signature_sha1);
    *signed_data = signature_data.message;

    return response;
  }

  // Mangles a string by inverting the first byte.
  static void MangleString(std::string* str) { (*str)[0] = ~(*str)[0]; }
};

// Note on expiration: VerifyCredentials() depends on the system clock. In
// practice this shouldn't be a problem though since the certificate chain
// being verified doesn't expire until 2032!
TEST_F(CastAuthUtilTest, VerifySuccess) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data);
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_TRUE(result.success());
  EXPECT_EQ(AuthResult::POLICY_NONE, result.channel_policies);
}

TEST_F(CastAuthUtilTest, VerifyBadCA) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data);
  MangleString(auth_response.mutable_intermediate_certificate(0));
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA, result.error_type);
}

TEST_F(CastAuthUtilTest, VerifyBadClientAuthCert) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data);
  MangleString(auth_response.mutable_client_auth_certificate());
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  // TODO(eroman): Not quite right of an error.
  EXPECT_EQ(AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA, result.error_type);
}

TEST_F(CastAuthUtilTest, VerifyBadSignature) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data);
  MangleString(auth_response.mutable_signature());
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_SIGNED_BLOBS_MISMATCH, result.error_type);
}

TEST_F(CastAuthUtilTest, VerifyBadPeerCert) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data);
  MangleString(&signed_data);
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_SIGNED_BLOBS_MISMATCH, result.error_type);
}

}  // namespace
}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
