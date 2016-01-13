// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_secret_boxer.h"

#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/quic_random.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::string;

namespace net {
namespace test {

TEST(CryptoSecretBoxerTest, BoxAndUnbox) {
  StringPiece message("hello world");
  const size_t key_size = CryptoSecretBoxer::GetKeySize();
  scoped_ptr<uint8[]> key(new uint8[key_size]);
  memset(key.get(), 0x11, key_size);

  CryptoSecretBoxer boxer;
  boxer.SetKey(StringPiece(reinterpret_cast<char*>(key.get()), key_size));

  const string box = boxer.Box(QuicRandom::GetInstance(), message);

  string storage;
  StringPiece result;
  EXPECT_TRUE(boxer.Unbox(box, &storage, &result));
  EXPECT_EQ(result, message);

  EXPECT_FALSE(boxer.Unbox(string(1, 'X') + box, &storage, &result));
  EXPECT_FALSE(boxer.Unbox(box.substr(1, string::npos), &storage, &result));
  EXPECT_FALSE(boxer.Unbox(string(), &storage, &result));
  EXPECT_FALSE(boxer.Unbox(string(1, box[0]^0x80) + box.substr(1, string::npos),
                           &storage, &result));
}

}  // namespace test
}  // namespace net
