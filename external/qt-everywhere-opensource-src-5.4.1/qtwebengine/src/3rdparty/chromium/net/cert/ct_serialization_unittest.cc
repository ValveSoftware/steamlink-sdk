// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_serialization.h"

#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "net/base/net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class CtSerializationTest : public ::testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    test_digitally_signed_ = ct::GetTestDigitallySigned();
  }

 protected:
  std::string test_digitally_signed_;
};

TEST_F(CtSerializationTest, DecodesDigitallySigned) {
  base::StringPiece digitally_signed(test_digitally_signed_);
  ct::DigitallySigned parsed;

  ASSERT_TRUE(ct::DecodeDigitallySigned(&digitally_signed, &parsed));
  EXPECT_EQ(
      ct::DigitallySigned::HASH_ALGO_SHA256,
      parsed.hash_algorithm);

  EXPECT_EQ(
      ct::DigitallySigned::SIG_ALGO_ECDSA,
      parsed.signature_algorithm);

  // The encoded data contains the signature itself from the 4th byte.
  // The first bytes are:
  // 1 byte of hash algorithm
  // 1 byte of signature algorithm
  // 2 bytes - prefix containing length of the signature data.
  EXPECT_EQ(
      test_digitally_signed_.substr(4),
      parsed.signature_data);
}


TEST_F(CtSerializationTest, FailsToDecodePartialDigitallySigned) {
  base::StringPiece digitally_signed(test_digitally_signed_);
  base::StringPiece partial_digitally_signed(
      digitally_signed.substr(0, test_digitally_signed_.size() - 5));
  ct::DigitallySigned parsed;

  ASSERT_FALSE(ct::DecodeDigitallySigned(&partial_digitally_signed, &parsed));
}


TEST_F(CtSerializationTest, EncodesDigitallySigned) {
  ct::DigitallySigned digitally_signed;
  digitally_signed.hash_algorithm = ct::DigitallySigned::HASH_ALGO_SHA256;
  digitally_signed.signature_algorithm = ct::DigitallySigned::SIG_ALGO_ECDSA;
  digitally_signed.signature_data = test_digitally_signed_.substr(4);

  std::string encoded;

  ASSERT_TRUE(ct::EncodeDigitallySigned(digitally_signed, &encoded));
  EXPECT_EQ(test_digitally_signed_, encoded);
}


TEST_F(CtSerializationTest, EncodesLogEntryForX509Cert) {
  ct::LogEntry entry;
  GetX509CertLogEntry(&entry);

  std::string encoded;
  ASSERT_TRUE(ct::EncodeLogEntry(entry, &encoded));
  EXPECT_EQ((718U + 5U), encoded.size());
  // First two bytes are log entry type. Next, length:
  // Length is 718 which is 512 + 206, which is 0x2ce
  std::string expected_prefix("\0\0\0\x2\xCE", 5);
  // Note we use std::string comparison rather than ASSERT_STREQ due
  // to null characters in the buffer.
  EXPECT_EQ(expected_prefix, encoded.substr(0, 5));
}

TEST_F(CtSerializationTest, EncodesV1SCTSignedData) {
  base::Time timestamp = base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(1348589665525);
  std::string dummy_entry("abc");
  std::string empty_extensions;
  // For now, no known failure cases.
  std::string encoded;
  ASSERT_TRUE(ct::EncodeV1SCTSignedData(
      timestamp,
      dummy_entry,
      empty_extensions,
      &encoded));
  EXPECT_EQ((size_t) 15, encoded.size());
  // Byte 0 is version, byte 1 is signature type
  // Bytes 2-10 are timestamp
  // Bytes 11-14 are the log signature
  // Byte 15 is the empty extension
  //EXPECT_EQ(0, timestamp.ToTimeT());
  std::string expected_buffer(
      "\x0\x0\x0\x0\x1\x39\xFE\x35\x3C\xF5\x61\x62\x63\x0\x0", 15);
  EXPECT_EQ(expected_buffer, encoded);
}

TEST_F(CtSerializationTest, DecodesSCTList) {
  // Two items in the list: "abc", "def"
  base::StringPiece encoded("\x0\xa\x0\x3\x61\x62\x63\x0\x3\x64\x65\x66", 12);
  std::vector<base::StringPiece> decoded;

  ASSERT_TRUE(ct::DecodeSCTList(&encoded, &decoded));
  ASSERT_STREQ("abc", decoded[0].data());
  ASSERT_STREQ("def", decoded[1].data());
}

TEST_F(CtSerializationTest, FailsDecodingInvalidSCTList) {
  // A list with one item that's too short
  base::StringPiece encoded("\x0\xa\x0\x3\x61\x62\x63\x0\x5\x64\x65\x66", 12);
  std::vector<base::StringPiece> decoded;

  ASSERT_FALSE(ct::DecodeSCTList(&encoded, &decoded));
}

TEST_F(CtSerializationTest, DecodesSignedCertificateTimestamp) {
  std::string encoded_test_sct(ct::GetTestSignedCertificateTimestamp());
  base::StringPiece encoded_sct(encoded_test_sct);

  scoped_refptr<ct::SignedCertificateTimestamp> sct;
  ASSERT_TRUE(ct::DecodeSignedCertificateTimestamp(&encoded_sct, &sct));
  EXPECT_EQ(0, sct->version);
  EXPECT_EQ(ct::GetTestPublicKeyId(), sct->log_id);
  base::Time expected_time = base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(1365181456089);
  EXPECT_EQ(expected_time, sct->timestamp);
  // Subtracting 4 bytes for signature data (hash & sig algs),
  // actual signature data should be 71 bytes.
  EXPECT_EQ((size_t) 71, sct->signature.signature_data.size());
  EXPECT_TRUE(sct->extensions.empty());
}

TEST_F(CtSerializationTest, FailsDecodingInvalidSignedCertificateTimestamp) {
  // Invalid version
  base::StringPiece invalid_version_sct("\x2\x0", 2);
  scoped_refptr<ct::SignedCertificateTimestamp> sct;

  ASSERT_FALSE(
      ct::DecodeSignedCertificateTimestamp(&invalid_version_sct, &sct));

  // Valid version, invalid length (missing data)
  base::StringPiece invalid_length_sct("\x0\xa\xb\xc", 4);
  ASSERT_FALSE(
      ct::DecodeSignedCertificateTimestamp(&invalid_length_sct, &sct));
}

TEST_F(CtSerializationTest, EncodesValidSignedTreeHead) {
  ct::SignedTreeHead signed_tree_head;
  GetSignedTreeHead(&signed_tree_head);

  std::string encoded;
  ct::EncodeTreeHeadSignature(signed_tree_head, &encoded);
  // Expected size is 50 bytes:
  // Byte 0 is version, byte 1 is signature type
  // Bytes 2-9 are timestamp
  // Bytes 10-17 are tree size
  // Bytes 18-49 are sha256 root hash
  ASSERT_EQ(50u, encoded.length());
  std::string expected_buffer(
      "\x0\x1\x0\x0\x1\x45\x3c\x5f\xb8\x35\x0\x0\x0\x0\x0\x0\x0\x15", 18);
  expected_buffer.append(ct::GetSampleSTHSHA256RootHash());
  ASSERT_EQ(expected_buffer, encoded);
}

}  // namespace net

