// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/pref_service_hash_store_contents.h"

#include <string>

#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefServiceHashStoreContentsTest : public testing::Test {
 public:
  void SetUp() override {
    PrefServiceHashStoreContents::RegisterPrefs(local_state_.registry());
  }

 protected:
  TestingPrefServiceSimple local_state_;
};

TEST_F(PrefServiceHashStoreContentsTest, hash_store_id) {
  PrefServiceHashStoreContents contents("store_id", &local_state_);
  ASSERT_EQ("store_id", contents.hash_store_id());
}

TEST_F(PrefServiceHashStoreContentsTest, IsInitialized) {
  {
    PrefServiceHashStoreContents contents("store_id", &local_state_);
    ASSERT_FALSE(contents.IsInitialized());
    (*contents.GetMutableContents())->Set("foo", new base::StringValue("bar"));
    ASSERT_TRUE(contents.IsInitialized());
  }
  {
    PrefServiceHashStoreContents contents("store_id", &local_state_);
    ASSERT_TRUE(contents.IsInitialized());
    PrefServiceHashStoreContents other_contents("other_store_id",
                                                &local_state_);
    ASSERT_FALSE(other_contents.IsInitialized());
  }
}

TEST_F(PrefServiceHashStoreContentsTest, Reset) {
  ASSERT_FALSE(local_state_.GetUserPrefValue(
      PrefServiceHashStoreContents::kProfilePreferenceHashes));

  {
    PrefServiceHashStoreContents contents("store_id", &local_state_);
    ASSERT_FALSE(contents.IsInitialized());
    (*contents.GetMutableContents())->Set("foo", new base::StringValue("bar"));
    ASSERT_TRUE(contents.IsInitialized());
    PrefServiceHashStoreContents other_contents("other_store_id",
                                                &local_state_);
    (*other_contents.GetMutableContents())
        ->Set("foo", new base::StringValue("bar"));
  }

  ASSERT_TRUE(local_state_.GetUserPrefValue(
      PrefServiceHashStoreContents::kProfilePreferenceHashes));

  {
    PrefServiceHashStoreContents contents("store_id", &local_state_);
    ASSERT_TRUE(contents.IsInitialized());
    contents.Reset();
    ASSERT_FALSE(contents.IsInitialized());
  }

  ASSERT_TRUE(local_state_.GetUserPrefValue(
      PrefServiceHashStoreContents::kProfilePreferenceHashes));

  {
    PrefServiceHashStoreContents contents("store_id", &local_state_);
    ASSERT_FALSE(contents.IsInitialized());
    PrefServiceHashStoreContents other_contents("other_store_id",
                                                &local_state_);
    ASSERT_TRUE(other_contents.IsInitialized());
  }

  {
    PrefServiceHashStoreContents other_contents("other_store_id",
                                                &local_state_);
    other_contents.Reset();
  }

  ASSERT_FALSE(local_state_.GetUserPrefValue(
      PrefServiceHashStoreContents::kProfilePreferenceHashes));
}

TEST_F(PrefServiceHashStoreContentsTest, GetAndSetContents) {
  {
    PrefServiceHashStoreContents contents("store_id", &local_state_);
    ASSERT_EQ(NULL, contents.GetContents());
    (*contents.GetMutableContents())->Set("foo", new base::StringValue("bar"));
    ASSERT_FALSE(contents.GetContents() == NULL);
    std::string actual_value;
    ASSERT_TRUE(contents.GetContents()->GetString("foo", &actual_value));
    ASSERT_EQ("bar", actual_value);
    PrefServiceHashStoreContents other_contents("other_store_id",
                                                &local_state_);
    ASSERT_EQ(NULL, other_contents.GetContents());
  }
  {
    PrefServiceHashStoreContents contents("store_id", &local_state_);
    ASSERT_FALSE(contents.GetContents() == NULL);
  }
}

TEST_F(PrefServiceHashStoreContentsTest, GetAndSetSuperMac) {
  {
    PrefServiceHashStoreContents contents("store_id", &local_state_);
    ASSERT_TRUE(contents.GetSuperMac().empty());
    (*contents.GetMutableContents())->Set("foo", new base::StringValue("bar"));
    ASSERT_TRUE(contents.GetSuperMac().empty());
    contents.SetSuperMac("0123456789");
    ASSERT_EQ("0123456789", contents.GetSuperMac());
  }
  {
    PrefServiceHashStoreContents contents("store_id", &local_state_);
    ASSERT_EQ("0123456789", contents.GetSuperMac());
    PrefServiceHashStoreContents other_contents("other_store_id",
                                                &local_state_);
    ASSERT_TRUE(other_contents.GetSuperMac().empty());
  }
}

TEST_F(PrefServiceHashStoreContentsTest, ResetAllPrefHashStores) {
  {
    PrefServiceHashStoreContents contents_1("store_id_1", &local_state_);
    PrefServiceHashStoreContents contents_2("store_id_2", &local_state_);
    (*contents_1.GetMutableContents())
        ->Set("foo", new base::StringValue("bar"));
    (*contents_2.GetMutableContents())
        ->Set("foo", new base::StringValue("bar"));
  }
  {
    PrefServiceHashStoreContents contents_1("store_id_1", &local_state_);
    PrefServiceHashStoreContents contents_2("store_id_2", &local_state_);
    ASSERT_TRUE(contents_1.IsInitialized());
    ASSERT_TRUE(contents_2.IsInitialized());
  }

  PrefServiceHashStoreContents::ResetAllPrefHashStores(&local_state_);

  {
    PrefServiceHashStoreContents contents_1("store_id_1", &local_state_);
    PrefServiceHashStoreContents contents_2("store_id_2", &local_state_);
    ASSERT_FALSE(contents_1.IsInitialized());
    ASSERT_FALSE(contents_2.IsInitialized());
  }
}
