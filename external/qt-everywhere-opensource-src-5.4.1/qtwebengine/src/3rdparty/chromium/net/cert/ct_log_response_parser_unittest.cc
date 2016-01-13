// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_response_parser.h"

#include <string>

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_tree_head.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace ct {

std::string CreateSignedTreeHeadJsonString(std::string sha256_root_hash,
                                           std::string tree_head_signature) {
  std::string sth_json = "{\"tree_size\":2903698,\"timestamp\":1395761621447";

  if (!sha256_root_hash.empty()) {
    sth_json += base::StringPrintf(",\"sha256_root_hash\":\"%s\"",
                                   sha256_root_hash.c_str());
  }
  if (!tree_head_signature.empty()) {
    sth_json += base::StringPrintf(",\"tree_head_signature\":\"%s\"",
                                   tree_head_signature.c_str());
  }

  sth_json += "}";
  return sth_json;
}

const char kSHA256RootHash[] = "/WHFMgXtI/umKKuACJIN0Bb73TcILm9WkeU6qszvoAo=";

const char kTreeHeadSignature[] =
    "BAMARzBFAiAB+IIYrkRsZDW0/6TzPgR+aJ26twCQ1JDTwq/"
    "mpinCjAIhAKDXdXMtqbvQ42r9dBIwV5RM/KpEzNQdIhXHesd9HPv3";

TEST(CTLogResponseParserTest, ParsesValidJsonSTH) {
  std::string sample_sth =
      CreateSignedTreeHeadJsonString(kSHA256RootHash, kTreeHeadSignature);
  SignedTreeHead tree_head;
  EXPECT_TRUE(FillSignedTreeHead(sample_sth, &tree_head));

  base::Time expected_timestamp =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(1395761621447);

  ASSERT_EQ(SignedTreeHead::V1, tree_head.version);
  ASSERT_EQ(expected_timestamp, tree_head.timestamp);
  ASSERT_EQ(2903698u, tree_head.tree_size);

  // Copy the field from the SignedTreeHead because it's not null terminated
  // there and ASSERT_STREQ expects null-terminated strings.
  char actual_hash[kSthRootHashLength + 1];
  memcpy(actual_hash, tree_head.sha256_root_hash, kSthRootHashLength);
  actual_hash[kSthRootHashLength] = '\0';
  std::string expected_sha256_root_hash;
  base::Base64Decode(kSHA256RootHash, &expected_sha256_root_hash);
  ASSERT_STREQ(expected_sha256_root_hash.c_str(), actual_hash);

  std::string tree_head_signature;
  base::Base64Decode(kTreeHeadSignature, &tree_head_signature);
  base::StringPiece sp(tree_head_signature);
  DigitallySigned expected_signature;
  ASSERT_TRUE(DecodeDigitallySigned(&sp, &expected_signature));

  ASSERT_EQ(tree_head.signature.hash_algorithm,
            expected_signature.hash_algorithm);
  ASSERT_EQ(tree_head.signature.signature_algorithm,
            expected_signature.signature_algorithm);
  ASSERT_EQ(tree_head.signature.signature_data,
            expected_signature.signature_data);
}

TEST(CTLogResponseParserTest, FailsToParseMissingFields) {
  std::string missing_signature_sth =
      CreateSignedTreeHeadJsonString(kSHA256RootHash, "");

  SignedTreeHead tree_head;
  ASSERT_FALSE(FillSignedTreeHead(missing_signature_sth, &tree_head));

  std::string missing_root_hash_sth =
      CreateSignedTreeHeadJsonString("", kTreeHeadSignature);
  ASSERT_FALSE(FillSignedTreeHead(missing_root_hash_sth, &tree_head));
}

TEST(CTLogResponseParserTest, FailsToParseIncorrectLengthRootHash) {
  SignedTreeHead tree_head;

  std::string too_long_hash = CreateSignedTreeHeadJsonString(
      kSHA256RootHash, "/WHFMgXtI/umKKuACJIN0Bb73TcILm9WkeU6qszvoArK\n");
  ASSERT_FALSE(FillSignedTreeHead(too_long_hash, &tree_head));

  std::string too_short_hash = CreateSignedTreeHeadJsonString(
      kSHA256RootHash, "/WHFMgXtI/umKKuACJIN0Bb73TcILm9WkeU6qszvoA==\n");
  ASSERT_FALSE(FillSignedTreeHead(too_short_hash, &tree_head));
}

}  // namespace ct

}  // namespace net
