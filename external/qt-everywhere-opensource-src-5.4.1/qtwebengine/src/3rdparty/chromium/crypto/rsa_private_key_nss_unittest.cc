// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/rsa_private_key.h"

#include <keyhi.h>
#include <pk11pub.h>

#include "base/memory/scoped_ptr.h"
#include "crypto/nss_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto {

class RSAPrivateKeyNSSTest : public testing::Test {
 public:
  RSAPrivateKeyNSSTest() {}
  virtual ~RSAPrivateKeyNSSTest() {}

  virtual void SetUp() {
#if defined(OS_CHROMEOS)
    OpenPersistentNSSDB();
#endif
  }

 private:
  ScopedTestNSSDB test_nssdb_;

  DISALLOW_COPY_AND_ASSIGN(RSAPrivateKeyNSSTest);
};

TEST_F(RSAPrivateKeyNSSTest, CreateFromKeyTest) {
  scoped_ptr<crypto::RSAPrivateKey> key_pair(RSAPrivateKey::Create(256));

  scoped_ptr<crypto::RSAPrivateKey> key_copy(
      RSAPrivateKey::CreateFromKey(key_pair->key()));
  ASSERT_TRUE(key_copy.get());

  std::vector<uint8> privkey;
  std::vector<uint8> pubkey;
  ASSERT_TRUE(key_pair->ExportPrivateKey(&privkey));
  ASSERT_TRUE(key_pair->ExportPublicKey(&pubkey));

  std::vector<uint8> privkey_copy;
  std::vector<uint8> pubkey_copy;
  ASSERT_TRUE(key_copy->ExportPrivateKey(&privkey_copy));
  ASSERT_TRUE(key_copy->ExportPublicKey(&pubkey_copy));

  ASSERT_EQ(privkey, privkey_copy);
  ASSERT_EQ(pubkey, pubkey_copy);
}

TEST_F(RSAPrivateKeyNSSTest, FindFromPublicKey) {
  // Create a keypair, which will put the keys in the user's NSSDB.
  scoped_ptr<crypto::RSAPrivateKey> key_pair(RSAPrivateKey::Create(256));

  std::vector<uint8> public_key;
  ASSERT_TRUE(key_pair->ExportPublicKey(&public_key));

  scoped_ptr<crypto::RSAPrivateKey> key_pair_2(
      crypto::RSAPrivateKey::FindFromPublicKeyInfo(public_key));

  EXPECT_EQ(key_pair->key_->pkcs11ID, key_pair_2->key_->pkcs11ID);
}

TEST_F(RSAPrivateKeyNSSTest, FailedFindFromPublicKey) {
  // Create a keypair, which will put the keys in the user's NSSDB.
  scoped_ptr<crypto::RSAPrivateKey> key_pair(RSAPrivateKey::Create(256));

  std::vector<uint8> public_key;
  ASSERT_TRUE(key_pair->ExportPublicKey(&public_key));

  // Remove the keys from the DB, and make sure we can't find them again.
  if (key_pair->key_) {
    PK11_DestroyTokenObject(key_pair->key_->pkcs11Slot,
                            key_pair->key_->pkcs11ID);
  }
  if (key_pair->public_key_) {
    PK11_DestroyTokenObject(key_pair->public_key_->pkcs11Slot,
                            key_pair->public_key_->pkcs11ID);
  }

  EXPECT_EQ(NULL, crypto::RSAPrivateKey::FindFromPublicKeyInfo(public_key));
}

}  // namespace crypto
