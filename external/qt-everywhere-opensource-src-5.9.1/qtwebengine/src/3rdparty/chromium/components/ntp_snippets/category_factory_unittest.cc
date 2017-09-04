// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_factory.h"

#include <algorithm>
#include <vector>

#include "components/ntp_snippets/category.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

class CategoryFactoryTest : public testing::Test {
 public:
  CategoryFactoryTest()
      : unused_remote_category_id_(
            static_cast<int>(KnownCategories::LAST_KNOWN_REMOTE_CATEGORY) + 1) {
  }

  int GetUnusedRemoteCategoryID() { return unused_remote_category_id_++; }

  bool CompareCategories(const Category& left, const Category& right) {
    return factory()->CompareCategories(left, right);
  }

  void AddDummyRemoteCategories(int quantity) {
    for (int i = 0; i < quantity; ++i) {
      factory()->FromRemoteCategory(GetUnusedRemoteCategoryID());
    }
  }

  CategoryFactory* factory() { return &factory_; }

 private:
  CategoryFactory factory_;
  int unused_remote_category_id_;

  DISALLOW_COPY_AND_ASSIGN(CategoryFactoryTest);
};

TEST_F(CategoryFactoryTest,
       FromKnownCategoryShouldReturnSameIdForSameCategories) {
  const KnownCategories known_category = KnownCategories::BOOKMARKS;
  Category first = factory()->FromKnownCategory(known_category);
  Category second = factory()->FromKnownCategory(known_category);
  EXPECT_EQ(first, second);
}

TEST_F(CategoryFactoryTest,
       FromRemoteCategoryShouldReturnSameIdForSameCategories) {
  const int remote_category_id = GetUnusedRemoteCategoryID();
  Category first = factory()->FromRemoteCategory(remote_category_id);
  Category second = factory()->FromRemoteCategory(remote_category_id);
  EXPECT_EQ(first, second);
}

TEST_F(CategoryFactoryTest, FromRemoteCategoryOrder) {
  const int small_id = GetUnusedRemoteCategoryID();
  const int large_id = GetUnusedRemoteCategoryID();
  // Categories are added in decreasing id order to test that they are not
  // compared by id.
  Category added_first = factory()->FromRemoteCategory(large_id);
  Category added_second = factory()->FromRemoteCategory(small_id);
  EXPECT_TRUE(CompareCategories(added_first, added_second));
  EXPECT_FALSE(CompareCategories(added_second, added_first));
}

TEST_F(CategoryFactoryTest, FromIDValueReturnsSameKnownCategory) {
  Category known_category =
      factory()->FromKnownCategory(KnownCategories::BOOKMARKS);
  Category known_category_by_id = factory()->FromIDValue(known_category.id());
  EXPECT_EQ(known_category, known_category_by_id);
}

TEST_F(CategoryFactoryTest, FromIDValueReturnsSameRemoteCategory) {
  const int remote_category_id = GetUnusedRemoteCategoryID();
  Category remote_category = factory()->FromRemoteCategory(remote_category_id);
  Category remote_category_by_id = factory()->FromIDValue(remote_category.id());
  EXPECT_EQ(remote_category, remote_category_by_id);
}

TEST_F(CategoryFactoryTest, CompareCategoriesLocalBeforeRemote) {
  const int remote_category_id = GetUnusedRemoteCategoryID();
  Category remote_category = factory()->FromRemoteCategory(remote_category_id);
  Category local_category =
      factory()->FromKnownCategory(KnownCategories::BOOKMARKS);
  EXPECT_TRUE(CompareCategories(local_category, remote_category));
  EXPECT_FALSE(CompareCategories(remote_category, local_category));
}

TEST_F(CategoryFactoryTest, CompareCategoriesSame) {
  const int remote_category_id = GetUnusedRemoteCategoryID();
  Category remote_category = factory()->FromRemoteCategory(remote_category_id);
  EXPECT_FALSE(CompareCategories(remote_category, remote_category));

  Category local_category =
      factory()->FromKnownCategory(KnownCategories::BOOKMARKS);
  EXPECT_FALSE(CompareCategories(local_category, local_category));
}

TEST_F(CategoryFactoryTest, CompareCategoriesAfterAddingNew) {
  AddDummyRemoteCategories(3);

  Category consequtive_first =
      factory()->FromRemoteCategory(GetUnusedRemoteCategoryID());
  Category consequtive_second =
      factory()->FromRemoteCategory(GetUnusedRemoteCategoryID());

  AddDummyRemoteCategories(3);

  Category nonconsequtive_first =
      factory()->FromRemoteCategory(GetUnusedRemoteCategoryID());
  AddDummyRemoteCategories(3);
  Category nonconsequtive_second =
      factory()->FromRemoteCategory(GetUnusedRemoteCategoryID());

  AddDummyRemoteCategories(3);

  EXPECT_TRUE(CompareCategories(consequtive_first, consequtive_second));
  EXPECT_FALSE(CompareCategories(consequtive_second, consequtive_first));

  EXPECT_TRUE(CompareCategories(nonconsequtive_first, nonconsequtive_second));
  EXPECT_FALSE(CompareCategories(nonconsequtive_second, nonconsequtive_first));
}

}  // namespace ntp_snippets
