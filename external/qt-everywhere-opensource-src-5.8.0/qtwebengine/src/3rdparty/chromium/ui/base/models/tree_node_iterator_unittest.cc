// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/tree_node_iterator.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/tree_node_model.h"

namespace ui {

namespace {

typedef TreeNodeWithValue<int> TestNode;

bool PruneOdd(TestNode* node) {
  return node->value % 2;
}

bool PruneEven(TestNode* node) {
  return !PruneOdd(node);
}

TEST(TreeNodeIteratorTest, Basic) {
  TestNode root;
  root.Add(new TestNode(), 0);
  root.Add(new TestNode(), 1);
  TestNode* f3 = new TestNode();
  root.Add(f3, 2);
  TestNode* f4 = new TestNode();
  f3->Add(f4, 0);
  f4->Add(new TestNode(), 0);

  TreeNodeIterator<TestNode> iterator(&root);
  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.GetChild(0), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.GetChild(1), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.GetChild(2), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(f4, iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(f4->GetChild(0), iterator.Next());

  ASSERT_FALSE(iterator.has_next());
}

// The tree used for testing:
// * + 1
//   + 2
//   + 3 + 4 + 5
//       + 7
TEST(TreeNodeIteratorTest, Prune) {
  TestNode root;
  root.Add(new TestNode(1), 0);
  root.Add(new TestNode(2), 1);
  TestNode* f3 = new TestNode(3);
  root.Add(f3, 2);
  TestNode* f4 = new TestNode(4);
  f3->Add(f4, 0);
  f4->Add(new TestNode(5), 0);
  f3->Add(new TestNode(7), 1);

  TreeNodeIterator<TestNode> odd_iterator(&root, base::Bind(&PruneOdd));
  ASSERT_TRUE(odd_iterator.has_next());
  ASSERT_EQ(2, odd_iterator.Next()->value);
  ASSERT_FALSE(odd_iterator.has_next());

  TreeNodeIterator<TestNode> even_iterator(&root, base::Bind(&PruneEven));
  ASSERT_TRUE(even_iterator.has_next());
  ASSERT_EQ(1, even_iterator.Next()->value);
  ASSERT_TRUE(even_iterator.has_next());
  ASSERT_EQ(3, even_iterator.Next()->value);
  ASSERT_TRUE(even_iterator.has_next());
  ASSERT_EQ(7, even_iterator.Next()->value);
  ASSERT_FALSE(even_iterator.has_next());
}

}  // namespace

}  // namespace ui
