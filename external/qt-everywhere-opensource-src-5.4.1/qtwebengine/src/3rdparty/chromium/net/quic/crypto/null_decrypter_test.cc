// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/null_decrypter.h"
#include "net/quic/test_tools/quic_test_utils.h"

using base::StringPiece;

namespace net {
namespace test {

class NullDecrypterTest : public ::testing::TestWithParam<bool> {
};

TEST_F(NullDecrypterTest, Decrypt) {
  unsigned char expected[] = {
    // fnv hash
    0xa0, 0x6f, 0x44, 0x8a,
    0x44, 0xf8, 0x18, 0x3b,
    0x47, 0x91, 0xb2, 0x13,
    // payload
    'g',  'o',  'o',  'd',
    'b',  'y',  'e',  '!',
  };
  const char* data = reinterpret_cast<const char*>(expected);
  size_t len = arraysize(expected);
  NullDecrypter decrypter;
  scoped_ptr<QuicData> decrypted(
      decrypter.DecryptPacket(0, "hello world!", StringPiece(data, len)));
  ASSERT_TRUE(decrypted.get());
  EXPECT_EQ("goodbye!", decrypted->AsStringPiece());
}

TEST_F(NullDecrypterTest, BadHash) {
  unsigned char expected[] = {
    // fnv hash
    0x46, 0x11, 0xea, 0x5f,
    0xcf, 0x1d, 0x66, 0x5b,
    0xba, 0xf0, 0xbc, 0xfd,
    // payload
    'g',  'o',  'o',  'd',
    'b',  'y',  'e',  '!',
  };
  const char* data = reinterpret_cast<const char*>(expected);
  size_t len = arraysize(expected);
  NullDecrypter decrypter;
  scoped_ptr<QuicData> decrypted(
      decrypter.DecryptPacket(0, "hello world!", StringPiece(data, len)));
  ASSERT_FALSE(decrypted.get());
}

TEST_F(NullDecrypterTest, ShortInput) {
  unsigned char expected[] = {
    // fnv hash (truncated)
    0x46, 0x11, 0xea, 0x5f,
    0xcf, 0x1d, 0x66, 0x5b,
    0xba, 0xf0, 0xbc,
  };
  const char* data = reinterpret_cast<const char*>(expected);
  size_t len = arraysize(expected);
  NullDecrypter decrypter;
  scoped_ptr<QuicData> decrypted(
      decrypter.DecryptPacket(0, "hello world!", StringPiece(data, len)));
  ASSERT_FALSE(decrypted.get());
}

}  // namespace test
}  // namespace net
