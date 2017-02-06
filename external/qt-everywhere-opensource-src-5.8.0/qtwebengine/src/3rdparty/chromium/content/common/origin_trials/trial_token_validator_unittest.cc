// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/origin_trials/trial_token_validator.h"

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_trial_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebOriginTrialTokenStatus.h"
#include "url/gurl.h"

namespace content {

namespace {

// This is a sample public key for testing the API. The corresponding private
// key (use this to generate new samples for this test file) is:
//
//  0x83, 0x67, 0xf4, 0xcd, 0x2a, 0x1f, 0x0e, 0x04, 0x0d, 0x43, 0x13,
//  0x4c, 0x67, 0xc4, 0xf4, 0x28, 0xc9, 0x90, 0x15, 0x02, 0xe2, 0xba,
//  0xfd, 0xbb, 0xfa, 0xbc, 0x92, 0x76, 0x8a, 0x2c, 0x4b, 0xc7, 0x75,
//  0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2, 0x9a,
//  0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f, 0x64,
//  0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0
const uint8_t kTestPublicKey[] = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

//  0x21, 0xee, 0xfa, 0x81, 0x6a, 0xff, 0xdf, 0xb8, 0xc1, 0xdd, 0x75,
//  0x05, 0x04, 0x29, 0x68, 0x67, 0x60, 0x85, 0x91, 0xd0, 0x50, 0x16,
//  0x0a, 0xcf, 0xa2, 0x37, 0xa3, 0x2e, 0x11, 0x7a, 0x17, 0x96, 0x50,
//  0x07, 0x4d, 0x76, 0x55, 0x56, 0x42, 0x17, 0x2d, 0x8a, 0x9c, 0x47,
//  0x96, 0x25, 0xda, 0x70, 0xaa, 0xb9, 0xfd, 0x53, 0x5d, 0x51, 0x3e,
//  0x16, 0xab, 0xb4, 0x86, 0xea, 0xf3, 0x35, 0xc6, 0xca
const uint8_t kTestPublicKey2[] = {
    0x50, 0x07, 0x4d, 0x76, 0x55, 0x56, 0x42, 0x17, 0x2d, 0x8a, 0x9c,
    0x47, 0x96, 0x25, 0xda, 0x70, 0xaa, 0xb9, 0xfd, 0x53, 0x5d, 0x51,
    0x3e, 0x16, 0xab, 0xb4, 0x86, 0xea, 0xf3, 0x35, 0xc6, 0xca,
};

// This is a good trial token, signed with the above test private key.
// TODO(iclelland): This token expires in 2033. Update it or find a way
// to autogenerate it before then.
// Generate this token with the command (in tools/origin_trials):
// generate_token.py valid.example.com Frobulate --expire-timestamp=2000000000
const char kSampleToken[] =
    "AuR/1mg+/w5ROLN54Ok20rApK3opgR7Tq9ZfzhATQmnCa+BtPA1RRw4Nigf336r+"
    "O4fM3Sa+MEd+5JcIgSZafw8AAABZeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMjAwMDAwMDAwMH0=";

// The token should be valid for this origin and for this feature.
const char kAppropriateOrigin[] = "https://valid.example.com";
const char kAppropriateFeatureName[] = "Frobulate";

const char kInappropriateFeatureName[] = "Grokalyze";
const char kInappropriateOrigin[] = "https://invalid.example.com";
const char kInsecureOrigin[] = "http://valid.example.com";

// Well-formed trial token with an invalid signature.
// This token is a corruption of the above valid token.
const char kInvalidSignatureToken[] =
    "AuR/1mg+/w5ROLN54Ok20rApK3opgR7Tq9ZfzhATQmnCa+BtPA1RRw4Nigf336r+"
    "RrOtlAwa0gPqqn+A8GTD3AQAAABZeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMjAwMDAwMDAwMH0=";

// Well-formed, but expired, trial token. (Expired in 2001)
// Generate this token with the command (in tools/origin_trials):
// generate_token.py valid.example.com Frobulate --expire-timestamp=1000000000
const char kExpiredToken[] =
    "AmHPUIXMaXe9jWW8kJeDFXolVjT93p4XMnK4+jMYd2pjqtFcYB1bUmdD8PunQKM+"
    "RrOtlAwa0gPqqn+A8GTD3AQAAABZeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMTAwMDAwMDAwMH0=";

const char kUnparsableToken[] = "abcde";

class TestOriginTrialPolicy : public OriginTrialPolicy {
 public:
  base::StringPiece GetPublicKey() const override {
    return base::StringPiece(reinterpret_cast<const char*>(key_),
                             arraysize(kTestPublicKey));
  }
  bool IsFeatureDisabled(base::StringPiece feature) const override {
    return disabled_features_.count(feature.as_string()) > 0;
  }

  // Test setup methods
  void SetPublicKey(const uint8_t* key) { key_ = key; }
  void DisableFeature(const std::string& feature) {
    disabled_features_.insert(feature);
  }

 private:
  const uint8_t* key_ = nullptr;
  std::set<std::string> disabled_features_;
};

class TestContentClient : public ContentClient {
 public:
  // ContentRendererClient methods
  OriginTrialPolicy* GetOriginTrialPolicy() override {
    return &origin_trial_policy_;
  }
  // Test setup methods
  void SetOriginTrialPublicKey(const uint8_t* key) {
    origin_trial_policy_.SetPublicKey(key);
  }
  void DisableFeature(const std::string& feature) {
    origin_trial_policy_.DisableFeature(feature);
  }

 private:
  TestOriginTrialPolicy origin_trial_policy_;
};

}  // namespace

class TrialTokenValidatorTest : public testing::Test {
 public:
  TrialTokenValidatorTest()
      : appropriate_origin_(GURL(kAppropriateOrigin)),
        inappropriate_origin_(GURL(kInappropriateOrigin)),
        insecure_origin_(GURL(kInsecureOrigin)) {
    SetPublicKey(kTestPublicKey);
    SetContentClient(&test_content_client_);
  }

  ~TrialTokenValidatorTest() override { SetContentClient(nullptr); }

  void SetPublicKey(const uint8_t* key) {
    test_content_client_.SetOriginTrialPublicKey(key);
  }

  void DisableFeature(const std::string& feature) {
    test_content_client_.DisableFeature(feature);
  }

  const url::Origin appropriate_origin_;
  const url::Origin inappropriate_origin_;
  const url::Origin insecure_origin_;

 private:
  TestContentClient test_content_client_;
};

TEST_F(TrialTokenValidatorTest, ValidateValidToken) {
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Success,
            TrialTokenValidator::ValidateToken(
                kSampleToken, appropriate_origin_, kAppropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateInappropriateOrigin) {
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongOrigin,
            TrialTokenValidator::ValidateToken(
                kSampleToken, inappropriate_origin_, kAppropriateFeatureName));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongOrigin,
            TrialTokenValidator::ValidateToken(kSampleToken, insecure_origin_,
                                               kAppropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateInappropriateFeature) {
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongFeature,
            TrialTokenValidator::ValidateToken(
                kSampleToken, appropriate_origin_, kInappropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateInvalidSignature) {
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::InvalidSignature,
            TrialTokenValidator::ValidateToken(kInvalidSignatureToken,
                                               appropriate_origin_,
                                               kAppropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateUnparsableToken) {
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Malformed,
            TrialTokenValidator::ValidateToken(kUnparsableToken,
                                               appropriate_origin_,
                                               kAppropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateExpiredToken) {
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Expired,
            TrialTokenValidator::ValidateToken(
                kExpiredToken, appropriate_origin_, kAppropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateValidTokenWithIncorrectKey) {
  SetPublicKey(kTestPublicKey2);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::InvalidSignature,
            TrialTokenValidator::ValidateToken(
                kSampleToken, appropriate_origin_, kAppropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidatorRespectsDisabledFeatures) {
  // Disable an irrelevant feature; token should still validate
  DisableFeature(kInappropriateFeatureName);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Success,
            TrialTokenValidator::ValidateToken(
                kSampleToken, appropriate_origin_, kAppropriateFeatureName));
  // Disable the token's feature; it should no longer be valid
  DisableFeature(kAppropriateFeatureName);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::FeatureDisabled,
            TrialTokenValidator::ValidateToken(
                kSampleToken, appropriate_origin_, kAppropriateFeatureName));
}

}  // namespace content
