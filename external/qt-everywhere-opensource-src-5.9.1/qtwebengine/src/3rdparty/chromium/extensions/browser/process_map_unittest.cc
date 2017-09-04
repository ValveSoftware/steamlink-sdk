// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map.h"

#include "testing/gtest/include/gtest/gtest.h"

using extensions::ProcessMap;

TEST(ExtensionProcessMapTest, Test) {
  ProcessMap map;

  // Test behavior when empty.
  EXPECT_FALSE(map.Contains("a", 1));
  EXPECT_FALSE(map.Remove("a", 1, 1));
  EXPECT_EQ(0u, map.size());

  // Test insertion and behavior with one item.
  EXPECT_TRUE(map.Insert("a", 1, 1));
  EXPECT_TRUE(map.Contains("a", 1));
  EXPECT_FALSE(map.Contains("a", 2));
  EXPECT_FALSE(map.Contains("b", 1));
  EXPECT_EQ(1u, map.size());

  // Test inserting a duplicate item.
  EXPECT_FALSE(map.Insert("a", 1, 1));
  EXPECT_TRUE(map.Contains("a", 1));
  EXPECT_EQ(1u, map.size());

  // Insert some more items.
  EXPECT_TRUE(map.Insert("a", 2, 2));
  EXPECT_TRUE(map.Insert("b", 1, 3));
  EXPECT_TRUE(map.Insert("b", 2, 4));
  EXPECT_EQ(4u, map.size());

  EXPECT_TRUE(map.Contains("a", 1));
  EXPECT_TRUE(map.Contains("a", 2));
  EXPECT_TRUE(map.Contains("b", 1));
  EXPECT_TRUE(map.Contains("b", 2));
  EXPECT_FALSE(map.Contains("a", 3));

  // Note that this only differs from an existing item because of the site
  // instance id.
  EXPECT_TRUE(map.Insert("a", 1, 5));
  EXPECT_TRUE(map.Contains("a", 1));

  // Test removal.
  EXPECT_TRUE(map.Remove("a", 1, 1));
  EXPECT_FALSE(map.Remove("a", 1, 1));
  EXPECT_EQ(4u, map.size());

  // Should still return true because there were two site instances for this
  // extension/process pair.
  EXPECT_TRUE(map.Contains("a", 1));

  EXPECT_TRUE(map.Remove("a", 1, 5));
  EXPECT_EQ(3u, map.size());
  EXPECT_FALSE(map.Contains("a", 1));

  EXPECT_EQ(2, map.RemoveAllFromProcess(2));
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(0, map.RemoveAllFromProcess(2));
  EXPECT_EQ(1u, map.size());
}
