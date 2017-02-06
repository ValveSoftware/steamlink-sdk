// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "components/os_crypt/key_storage_linux.h"
#include "components/os_crypt/os_crypt.h"
#include "components/os_crypt/os_crypt_mocker_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class OSCryptLinuxTest : public testing::Test {
 public:
  OSCryptLinuxTest() = default;
  ~OSCryptLinuxTest() override = default;

  void SetUp() override {
    OSCryptMockerLinux::SetUpWithSingleton();
    key_storage_ = OSCryptMockerLinux::GetInstance();
  }

  void TearDown() override { OSCryptMockerLinux::TearDown(); }

 protected:
  OSCryptMockerLinux* key_storage_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(OSCryptLinuxTest);
};

TEST_F(OSCryptLinuxTest, VerifyV0) {
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  key_storage_->ResetTo("");
  ciphertext = originaltext;  // No encryption
  ASSERT_TRUE(OSCrypt::DecryptString(ciphertext, &decipheredtext));
  ASSERT_EQ(originaltext, decipheredtext);
}

TEST_F(OSCryptLinuxTest, VerifyV10) {
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  key_storage_->ResetTo("peanuts");
  ASSERT_TRUE(OSCrypt::EncryptString(originaltext, &ciphertext));
  key_storage_->ResetTo("not_peanuts");
  ciphertext = ciphertext.substr(3).insert(0, "v10");
  ASSERT_TRUE(OSCrypt::DecryptString(ciphertext, &decipheredtext));
  ASSERT_EQ(originaltext, decipheredtext);
}

TEST_F(OSCryptLinuxTest, VerifyV11) {
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  key_storage_->ResetTo("");
  ASSERT_TRUE(OSCrypt::EncryptString(originaltext, &ciphertext));
  ASSERT_EQ(ciphertext.substr(0, 3), "v11");
  ASSERT_TRUE(OSCrypt::DecryptString(ciphertext, &decipheredtext));
  ASSERT_EQ(originaltext, decipheredtext);
}

}  // namespace
