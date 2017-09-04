// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/registry_hash_store_contents_win.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr base::char16 kRegistryPath[] = L"Foo\\TestStore";
constexpr base::char16 kStoreKey[] = L"test_store_key";

// Hex-encoded MACs are 64 characters long.
constexpr char kTestStringA[] =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kTestStringB[] =
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

constexpr char kAtomicPrefPath[] = "path1";
constexpr char kSplitPrefPath[] = "extension";

class RegistryHashStoreContentsWinTest : public testing::Test {
 protected:
  RegistryHashStoreContentsWinTest() {}

  void SetUp() override {
    registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER);

    contents.reset(new RegistryHashStoreContentsWin(kRegistryPath, kStoreKey));
  }

  std::unique_ptr<HashStoreContents> contents;

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;

  DISALLOW_COPY_AND_ASSIGN(RegistryHashStoreContentsWinTest);
};

}  // namespace

TEST_F(RegistryHashStoreContentsWinTest, TestSetAndGetMac) {
  std::string stored_mac;
  EXPECT_FALSE(contents->GetMac(kAtomicPrefPath, &stored_mac));

  contents->SetMac(kAtomicPrefPath, kTestStringA);

  EXPECT_TRUE(contents->GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_EQ(kTestStringA, stored_mac);
}

TEST_F(RegistryHashStoreContentsWinTest, TestSetAndGetSplitMacs) {
  std::map<std::string, std::string> split_macs;
  EXPECT_FALSE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));

  contents->SetSplitMac(kSplitPrefPath, "a", kTestStringA);
  contents->SetSplitMac(kSplitPrefPath, "b", kTestStringB);

  EXPECT_TRUE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));
  EXPECT_EQ(2U, split_macs.size());
  EXPECT_EQ(kTestStringA, split_macs.at("a"));
  EXPECT_EQ(kTestStringB, split_macs.at("b"));
}

TEST_F(RegistryHashStoreContentsWinTest, TestRemoveAtomicMac) {
  contents->SetMac(kAtomicPrefPath, kTestStringA);

  std::string stored_mac;
  EXPECT_TRUE(contents->GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_EQ(kTestStringA, stored_mac);

  contents->RemoveEntry(kAtomicPrefPath);

  EXPECT_FALSE(contents->GetMac(kAtomicPrefPath, &stored_mac));
}

TEST_F(RegistryHashStoreContentsWinTest, TestRemoveSplitMacs) {
  contents->SetSplitMac(kSplitPrefPath, "a", kTestStringA);
  contents->SetSplitMac(kSplitPrefPath, "b", kTestStringB);

  std::map<std::string, std::string> split_macs;
  EXPECT_TRUE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));
  EXPECT_EQ(2U, split_macs.size());

  contents->RemoveEntry(kSplitPrefPath);

  split_macs.clear();
  EXPECT_FALSE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));
  EXPECT_EQ(0U, split_macs.size());
}

TEST_F(RegistryHashStoreContentsWinTest, TestReset) {
  contents->SetMac(kAtomicPrefPath, kTestStringA);
  contents->SetSplitMac(kSplitPrefPath, "a", kTestStringA);

  std::string stored_mac;
  EXPECT_TRUE(contents->GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_EQ(kTestStringA, stored_mac);

  std::map<std::string, std::string> split_macs;
  EXPECT_TRUE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));
  EXPECT_EQ(1U, split_macs.size());

  contents->Reset();

  stored_mac.clear();
  EXPECT_FALSE(contents->GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_TRUE(stored_mac.empty());

  split_macs.clear();
  EXPECT_FALSE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));
  EXPECT_EQ(0U, split_macs.size());
}
