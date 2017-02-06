// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/origin_trials/trial_token.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
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

// This is a valid, but incorrect, public key for testing signatures against.
// The corresponding private key is:
//
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
// Generate this token with the command (in tools/origin_trials):
// generate_token.py valid.example.com Frobulate --expire-timestamp=1458766277
const char* kSampleToken =
    "Ap+Q/Qm0ELadZql+dlEGSwnAVsFZKgCEtUZg8idQC3uekkIeSZIY1tftoYdrwhqj"
    "7FO5L22sNvkZZnacLvmfNwsAAABZeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMTQ1ODc2NjI3N30=";

const char* kExpectedFeatureName = "Frobulate";
const char* kExpectedOrigin = "https://valid.example.com";
const uint64_t kExpectedExpiry = 1458766277;

// The token should not be valid for this origin, or for this feature.
const char* kInvalidOrigin = "https://invalid.example.com";
const char* kInsecureOrigin = "http://valid.example.com";
const char* kInvalidFeatureName = "Grokalyze";

// The token should be valid if the current time is kValidTimestamp or earlier.
double kValidTimestamp = 1458766276.0;

// The token should be invalid if the current time is kInvalidTimestamp or
// later.
double kInvalidTimestamp = 1458766278.0;

// Well-formed trial token with an invalid signature.
const char* kInvalidSignatureToken =
    "Ap+Q/Qm0ELadZql+dlEGSwnAVsFZKgCEtUZg8idQC3uekkIeSZIY1tftoYdrwhqj"
    "7FO5L22sNvkZZnacLvmfNwsAAABaeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGV4IiwgImV4cGly"
    "eSI6IDE0NTg3NjYyNzd9";

// Trial token truncated in the middle of the length field; too short to
// possibly be valid.
const char kTruncatedToken[] =
    "Ap+Q/Qm0ELadZql+dlEGSwnAVsFZKgCEtUZg8idQC3uekkIeSZIY1tftoYdrwhqj"
    "7FO5L22sNvkZZnacLvmfNwsA";

// Trial token with an incorrectly-declared length, but with a valid signature.
const char kIncorrectLengthToken[] =
    "Ao06eNl/CZuM88qurWKX4RfoVEpHcVHWxdOTrEXZkaC1GUHyb/8L4sthADiVWdc9"
    "kXFyF1BW5bbraqp6MBVr3wEAAABaeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMTQ1ODc2NjI3N30=";

// Trial token with a misidentified version (42).
const char kIncorrectVersionToken[] =
    "KlH8wVLT5o59uDvlJESorMDjzgWnvG1hmIn/GiT9Ng3f45ratVeiXCNTeaJheOaG"
    "A6kX4ir4Amv8aHVC+OJHZQkAAABZeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMTQ1ODc2NjI3N30=";

const char kSampleTokenJSON[] =
    "{\"origin\": \"https://valid.example.com:443\", \"feature\": "
    "\"Frobulate\", \"expiry\": 1458766277}";

// Various ill-formed trial tokens. These should all fail to parse.
const char* kInvalidTokens[] = {
    // Invalid - Not JSON at all
    "abcde",
    // Invalid JSON
    "{",
    // Not an object
    "\"abcde\"",
    "123.4",
    "[0, 1, 2]",
    // Missing keys
    "{}",
    "{\"something\": 1}",
    "{\"origin\": \"https://a.a\"}",
    "{\"origin\": \"https://a.a\", \"feature\": \"a\"}"
    "{\"origin\": \"https://a.a\", \"expiry\": 1458766277}",
    "{\"feature\": \"FeatureName\", \"expiry\": 1458766277}",
    // Incorrect types
    "{\"origin\": 1, \"feature\": \"a\", \"expiry\": 1458766277}",
    "{\"origin\": \"https://a.a\", \"feature\": 1, \"expiry\": 1458766277}",
    "{\"origin\": \"https://a.a\", \"feature\": \"a\", \"expiry\": \"1\"}",
    // Negative expiry timestamp
    "{\"origin\": \"https://a.a\", \"feature\": \"a\", \"expiry\": -1}",
    // Origin not a proper origin URL
    "{\"origin\": \"abcdef\", \"feature\": \"a\", \"expiry\": 1458766277}",
    "{\"origin\": \"data:text/plain,abcdef\", \"feature\": \"a\", \"expiry\": "
    "1458766277}",
    "{\"origin\": \"javascript:alert(1)\", \"feature\": \"a\", \"expiry\": "
    "1458766277}"};

}  // namespace

class TrialTokenTest : public testing::TestWithParam<const char*> {
 public:
  TrialTokenTest()
      : expected_origin_(GURL(kExpectedOrigin)),
        invalid_origin_(GURL(kInvalidOrigin)),
        insecure_origin_(GURL(kInsecureOrigin)),
        expected_expiry_(base::Time::FromDoubleT(kExpectedExpiry)),
        valid_timestamp_(base::Time::FromDoubleT(kValidTimestamp)),
        invalid_timestamp_(base::Time::FromDoubleT(kInvalidTimestamp)),
        correct_public_key_(
            base::StringPiece(reinterpret_cast<const char*>(kTestPublicKey),
                              arraysize(kTestPublicKey))),
        incorrect_public_key_(
            base::StringPiece(reinterpret_cast<const char*>(kTestPublicKey2),
                              arraysize(kTestPublicKey2))) {}

 protected:
  blink::WebOriginTrialTokenStatus Extract(const std::string& token_text,
                                           base::StringPiece public_key,
                                           std::string* token_payload) {
    return TrialToken::Extract(token_text, public_key, token_payload);
  }

  blink::WebOriginTrialTokenStatus ExtractIgnorePayload(
      const std::string& token_text,
      base::StringPiece public_key) {
    std::string token_payload;
    return Extract(token_text, public_key, &token_payload);
  }

  std::unique_ptr<TrialToken> Parse(const std::string& token_payload) {
    return TrialToken::Parse(token_payload);
  }

  bool ValidateOrigin(TrialToken* token, const url::Origin origin) {
    return token->ValidateOrigin(origin);
  }

  bool ValidateFeatureName(TrialToken* token, const char* feature_name) {
    return token->ValidateFeatureName(feature_name);
  }

  bool ValidateDate(TrialToken* token, const base::Time& now) {
    return token->ValidateDate(now);
  }

  base::StringPiece correct_public_key() { return correct_public_key_; }
  base::StringPiece incorrect_public_key() { return incorrect_public_key_; }

  const url::Origin expected_origin_;
  const url::Origin invalid_origin_;
  const url::Origin insecure_origin_;

  const base::Time expected_expiry_;
  const base::Time valid_timestamp_;
  const base::Time invalid_timestamp_;

 private:
  base::StringPiece correct_public_key_;
  base::StringPiece incorrect_public_key_;
};

// Test the extraction of the signed payload from token strings. This includes
// checking the included version identifier, payload length, and cryptographic
// signature.

// Test verification of signature and extraction of token JSON from signed
// token.
TEST_F(TrialTokenTest, ValidateValidSignature) {
  std::string token_payload;
  blink::WebOriginTrialTokenStatus status =
      Extract(kSampleToken, correct_public_key(), &token_payload);
  ASSERT_EQ(blink::WebOriginTrialTokenStatus::Success, status);
  EXPECT_STREQ(kSampleTokenJSON, token_payload.c_str());
}

TEST_F(TrialTokenTest, ValidateInvalidSignature) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload(kInvalidSignatureToken, correct_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::InvalidSignature, status);
}

TEST_F(TrialTokenTest, ValidateSignatureWithIncorrectKey) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload(kSampleToken, incorrect_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::InvalidSignature, status);
}

TEST_F(TrialTokenTest, ValidateEmptyToken) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload("", correct_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Malformed, status);
}

TEST_F(TrialTokenTest, ValidateShortToken) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload(kTruncatedToken, correct_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Malformed, status);
}

TEST_F(TrialTokenTest, ValidateUnsupportedVersion) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload(kIncorrectVersionToken, correct_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongVersion, status);
}

TEST_F(TrialTokenTest, ValidateSignatureWithIncorrectLength) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload(kIncorrectLengthToken, correct_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Malformed, status);
}

// Test parsing of fields from JSON token.

TEST_F(TrialTokenTest, ParseEmptyString) {
  std::unique_ptr<TrialToken> empty_token = Parse("");
  EXPECT_FALSE(empty_token);
}

TEST_P(TrialTokenTest, ParseInvalidString) {
  std::unique_ptr<TrialToken> empty_token = Parse(GetParam());
  EXPECT_FALSE(empty_token) << "Invalid trial token should not parse.";
}

INSTANTIATE_TEST_CASE_P(, TrialTokenTest, ::testing::ValuesIn(kInvalidTokens));

TEST_F(TrialTokenTest, ParseValidToken) {
  std::unique_ptr<TrialToken> token = Parse(kSampleTokenJSON);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_F(TrialTokenTest, ValidateValidToken) {
  std::unique_ptr<TrialToken> token = Parse(kSampleTokenJSON);
  ASSERT_TRUE(token);
  EXPECT_TRUE(ValidateOrigin(token.get(), expected_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), invalid_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), insecure_origin_));
  EXPECT_TRUE(ValidateFeatureName(token.get(), kExpectedFeatureName));
  EXPECT_FALSE(ValidateFeatureName(token.get(), kInvalidFeatureName));
  EXPECT_FALSE(ValidateFeatureName(
      token.get(), base::ToUpperASCII(kExpectedFeatureName).c_str()));
  EXPECT_FALSE(ValidateFeatureName(
      token.get(), base::ToLowerASCII(kExpectedFeatureName).c_str()));
  EXPECT_TRUE(ValidateDate(token.get(), valid_timestamp_));
  EXPECT_FALSE(ValidateDate(token.get(), invalid_timestamp_));
}

TEST_F(TrialTokenTest, TokenIsValidForFeature) {
  std::unique_ptr<TrialToken> token = Parse(kSampleTokenJSON);
  ASSERT_TRUE(token);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Success,
            token->IsValidForFeature(expected_origin_, kExpectedFeatureName,
                                     valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongFeature,
            token->IsValidForFeature(expected_origin_,
                                     base::ToUpperASCII(kExpectedFeatureName),
                                     valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongFeature,
            token->IsValidForFeature(expected_origin_,
                                     base::ToLowerASCII(kExpectedFeatureName),
                                     valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongOrigin,
            token->IsValidForFeature(invalid_origin_, kExpectedFeatureName,
                                     valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongOrigin,
            token->IsValidForFeature(insecure_origin_, kExpectedFeatureName,
                                     valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongFeature,
            token->IsValidForFeature(expected_origin_, kInvalidFeatureName,
                                     valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Expired,
            token->IsValidForFeature(expected_origin_, kExpectedFeatureName,
                                     invalid_timestamp_));
}

// Test overall extraction, to ensure output status matches returned token
TEST_F(TrialTokenTest, ExtractValidToken) {
  blink::WebOriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> token =
      TrialToken::From(kSampleToken, correct_public_key(), &status);
  EXPECT_TRUE(token);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Success, status);
}

TEST_F(TrialTokenTest, ExtractInvalidSignature) {
  blink::WebOriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> token =
      TrialToken::From(kSampleToken, incorrect_public_key(), &status);
  EXPECT_FALSE(token);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::InvalidSignature, status);
}

TEST_F(TrialTokenTest, ExtractMalformedToken) {
  blink::WebOriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> token =
      TrialToken::From(kIncorrectLengthToken, correct_public_key(), &status);
  EXPECT_FALSE(token);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Malformed, status);
}

}  // namespace content
