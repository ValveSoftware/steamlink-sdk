// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree_combiner.h"

namespace ui {

TEST(CombineAXTreesTest, RenumberOneTree) {
  AXTreeUpdate tree;
  tree.has_tree_data = true;
  tree.tree_data.tree_id = 1;
  tree.root_id = 2;
  tree.nodes.resize(3);
  tree.nodes[0].id = 2;
  tree.nodes[0].child_ids.push_back(4);
  tree.nodes[0].child_ids.push_back(6);
  tree.nodes[1].id = 4;
  tree.nodes[2].id = 6;

  AXTreeCombiner combiner;
  combiner.AddTree(tree, true);
  combiner.Combine();

  const AXTreeUpdate& combined = combiner.combined();

  EXPECT_EQ(1, combined.root_id);
  ASSERT_EQ(3U, combined.nodes.size());
  EXPECT_EQ(1, combined.nodes[0].id);
  ASSERT_EQ(2U, combined.nodes[0].child_ids.size());
  EXPECT_EQ(2, combined.nodes[0].child_ids[0]);
  EXPECT_EQ(3, combined.nodes[0].child_ids[1]);
  EXPECT_EQ(2, combined.nodes[1].id);
  EXPECT_EQ(3, combined.nodes[2].id);
}

TEST(CombineAXTreesTest, EmbedChildTree) {
  AXTreeUpdate parent_tree;
  parent_tree.root_id = 1;
  parent_tree.has_tree_data = true;
  parent_tree.tree_data.tree_id = 1;
  parent_tree.nodes.resize(3);
  parent_tree.nodes[0].id = 1;
  parent_tree.nodes[0].child_ids.push_back(2);
  parent_tree.nodes[0].child_ids.push_back(3);
  parent_tree.nodes[1].id = 2;
  parent_tree.nodes[1].role = AX_ROLE_BUTTON;
  parent_tree.nodes[2].id = 3;
  parent_tree.nodes[2].role = AX_ROLE_IFRAME;
  parent_tree.nodes[2].AddIntAttribute(AX_ATTR_CHILD_TREE_ID, 2);

  AXTreeUpdate child_tree;
  child_tree.root_id = 1;
  child_tree.has_tree_data = true;
  child_tree.tree_data.parent_tree_id = 1;
  child_tree.tree_data.tree_id = 2;
  child_tree.nodes.resize(3);
  child_tree.nodes[0].id = 1;
  child_tree.nodes[0].child_ids.push_back(2);
  child_tree.nodes[0].child_ids.push_back(3);
  child_tree.nodes[1].id = 2;
  child_tree.nodes[1].role = AX_ROLE_CHECK_BOX;
  child_tree.nodes[2].id = 3;
  child_tree.nodes[2].role = AX_ROLE_RADIO_BUTTON;

  AXTreeCombiner combiner;
  combiner.AddTree(parent_tree, true);
  combiner.AddTree(child_tree, false);
  combiner.Combine();

  const AXTreeUpdate& combined = combiner.combined();

  EXPECT_EQ(1, combined.root_id);
  ASSERT_EQ(6U, combined.nodes.size());
  EXPECT_EQ(1, combined.nodes[0].id);
  ASSERT_EQ(2U, combined.nodes[0].child_ids.size());
  EXPECT_EQ(2, combined.nodes[0].child_ids[0]);
  EXPECT_EQ(3, combined.nodes[0].child_ids[1]);
  EXPECT_EQ(2, combined.nodes[1].id);
  EXPECT_EQ(AX_ROLE_BUTTON, combined.nodes[1].role);
  EXPECT_EQ(3, combined.nodes[2].id);
  EXPECT_EQ(AX_ROLE_IFRAME, combined.nodes[2].role);
  EXPECT_EQ(1U, combined.nodes[2].child_ids.size());
  EXPECT_EQ(4, combined.nodes[2].child_ids[0]);
  EXPECT_EQ(4, combined.nodes[3].id);
  EXPECT_EQ(5, combined.nodes[4].id);
  EXPECT_EQ(AX_ROLE_CHECK_BOX, combined.nodes[4].role);
  EXPECT_EQ(6, combined.nodes[5].id);
  EXPECT_EQ(AX_ROLE_RADIO_BUTTON, combined.nodes[5].role);
}

TEST(CombineAXTreesTest, MapAllIdAttributes) {
  // This is a nonsensical accessibility tree, the goal is to make sure
  // that all attributes that reference IDs of other nodes are remapped.

  AXTreeUpdate tree;
  tree.has_tree_data = true;
  tree.tree_data.tree_id = 1;
  tree.root_id = 11;
  tree.nodes.resize(2);
  tree.nodes[0].id = 11;
  tree.nodes[0].child_ids.push_back(22);
  tree.nodes[0].AddIntAttribute(AX_ATTR_TABLE_HEADER_ID, 22);
  tree.nodes[0].AddIntAttribute(AX_ATTR_TABLE_ROW_HEADER_ID, 22);
  tree.nodes[0].AddIntAttribute(AX_ATTR_TABLE_COLUMN_HEADER_ID, 22);
  tree.nodes[0].AddIntAttribute(AX_ATTR_ACTIVEDESCENDANT_ID, 22);
  std::vector<int32_t> ids { 22 };
  tree.nodes[0].AddIntListAttribute(AX_ATTR_INDIRECT_CHILD_IDS, ids);
  tree.nodes[0].AddIntListAttribute(AX_ATTR_CONTROLS_IDS, ids);
  tree.nodes[0].AddIntListAttribute(AX_ATTR_DESCRIBEDBY_IDS, ids);
  tree.nodes[0].AddIntListAttribute(AX_ATTR_FLOWTO_IDS, ids);
  tree.nodes[0].AddIntListAttribute(AX_ATTR_LABELLEDBY_IDS, ids);
  tree.nodes[0].AddIntListAttribute(AX_ATTR_CELL_IDS, ids);
  tree.nodes[0].AddIntListAttribute(AX_ATTR_UNIQUE_CELL_IDS, ids);
  tree.nodes[1].id = 22;

  AXTreeCombiner combiner;
  combiner.AddTree(tree, true);
  combiner.Combine();

  const AXTreeUpdate& combined = combiner.combined();

  EXPECT_EQ(1, combined.root_id);
  ASSERT_EQ(2U, combined.nodes.size());
  EXPECT_EQ(1, combined.nodes[0].id);
  ASSERT_EQ(1U, combined.nodes[0].child_ids.size());
  EXPECT_EQ(2, combined.nodes[0].child_ids[0]);
  EXPECT_EQ(2, combined.nodes[1].id);

  EXPECT_EQ(2, combined.nodes[0].GetIntAttribute(AX_ATTR_TABLE_HEADER_ID));
  EXPECT_EQ(2, combined.nodes[0].GetIntAttribute(AX_ATTR_TABLE_ROW_HEADER_ID));
  EXPECT_EQ(2, combined.nodes[0].GetIntAttribute(
      AX_ATTR_TABLE_COLUMN_HEADER_ID));
  EXPECT_EQ(2, combined.nodes[0].GetIntAttribute(AX_ATTR_ACTIVEDESCENDANT_ID));
  EXPECT_EQ(2, combined.nodes[0].GetIntListAttribute(
      AX_ATTR_INDIRECT_CHILD_IDS)[0]);
  EXPECT_EQ(2, combined.nodes[0].GetIntListAttribute(
      AX_ATTR_CONTROLS_IDS)[0]);
  EXPECT_EQ(2, combined.nodes[0].GetIntListAttribute(
      AX_ATTR_DESCRIBEDBY_IDS)[0]);
  EXPECT_EQ(2, combined.nodes[0].GetIntListAttribute(
      AX_ATTR_FLOWTO_IDS)[0]);
  EXPECT_EQ(2, combined.nodes[0].GetIntListAttribute(
      AX_ATTR_LABELLEDBY_IDS)[0]);
  EXPECT_EQ(2, combined.nodes[0].GetIntListAttribute(
      AX_ATTR_CELL_IDS)[0]);
  EXPECT_EQ(2, combined.nodes[0].GetIntListAttribute(
      AX_ATTR_UNIQUE_CELL_IDS)[0]);
}

TEST(CombineAXTreesTest, Coordinates) {
  AXTreeUpdate parent_tree;
  parent_tree.has_tree_data = true;
  parent_tree.tree_data.tree_id = 1;
  parent_tree.root_id = 1;
  parent_tree.nodes.resize(3);
  parent_tree.nodes[0].id = 1;
  parent_tree.nodes[0].child_ids.push_back(2);
  parent_tree.nodes[0].child_ids.push_back(3);
  parent_tree.nodes[1].id = 2;
  parent_tree.nodes[1].role = AX_ROLE_BUTTON;
  parent_tree.nodes[1].location = gfx::Rect(50, 10, 200, 100);
  parent_tree.nodes[2].id = 3;
  parent_tree.nodes[2].role = AX_ROLE_IFRAME;
  parent_tree.nodes[2].AddIntAttribute(AX_ATTR_CHILD_TREE_ID, 2);

  AXTreeUpdate child_tree;
  child_tree.has_tree_data = true;
  child_tree.tree_data.parent_tree_id = 1;
  child_tree.tree_data.tree_id = 2;
  child_tree.root_id = 1;
  child_tree.nodes.resize(2);
  child_tree.nodes[0].id = 1;
  child_tree.nodes[0].child_ids.push_back(2);

  child_tree.nodes[0].transform.reset(new gfx::Transform());
  child_tree.nodes[0].transform->Translate(0, 300);
  child_tree.nodes[0].transform->Scale(2.0, 2.0);

  child_tree.nodes[1].id = 2;
  child_tree.nodes[1].role = AX_ROLE_BUTTON;
  child_tree.nodes[1].location = gfx::Rect(50, 10, 200, 100);

  AXTreeCombiner combiner;
  combiner.AddTree(parent_tree, true);
  combiner.AddTree(child_tree, false);
  combiner.Combine();

  const AXTreeUpdate& combined = combiner.combined();

  ASSERT_EQ(5U, combined.nodes.size());
  EXPECT_EQ(50, combined.nodes[1].location.x());
  EXPECT_EQ(10, combined.nodes[1].location.y());
  EXPECT_EQ(200, combined.nodes[1].location.width());
  EXPECT_EQ(100, combined.nodes[1].location.height());
  EXPECT_EQ(100, combined.nodes[4].location.x());
  EXPECT_EQ(320, combined.nodes[4].location.y());
  EXPECT_EQ(400, combined.nodes[4].location.width());
  EXPECT_EQ(200, combined.nodes[4].location.height());
}

TEST(CombineAXTreesTest, FocusedTree) {
  AXTreeUpdate parent_tree;
  parent_tree.has_tree_data = true;
  parent_tree.tree_data.tree_id = 1;
  parent_tree.tree_data.focused_tree_id = 2;
  parent_tree.tree_data.focus_id = 2;
  parent_tree.root_id = 1;
  parent_tree.nodes.resize(3);
  parent_tree.nodes[0].id = 1;
  parent_tree.nodes[0].child_ids.push_back(2);
  parent_tree.nodes[0].child_ids.push_back(3);
  parent_tree.nodes[1].id = 2;
  parent_tree.nodes[1].role = AX_ROLE_BUTTON;
  parent_tree.nodes[2].id = 3;
  parent_tree.nodes[2].role = AX_ROLE_IFRAME;
  parent_tree.nodes[2].AddIntAttribute(AX_ATTR_CHILD_TREE_ID, 2);

  AXTreeUpdate child_tree;
  child_tree.has_tree_data = true;
  child_tree.tree_data.parent_tree_id = 1;
  child_tree.tree_data.tree_id = 2;
  child_tree.tree_data.focus_id = 3;
  child_tree.root_id = 1;
  child_tree.nodes.resize(3);
  child_tree.nodes[0].id = 1;
  child_tree.nodes[0].child_ids.push_back(2);
  child_tree.nodes[0].child_ids.push_back(3);
  child_tree.nodes[1].id = 2;
  child_tree.nodes[1].role = AX_ROLE_CHECK_BOX;
  child_tree.nodes[2].id = 3;
  child_tree.nodes[2].role = AX_ROLE_RADIO_BUTTON;

  AXTreeCombiner combiner;
  combiner.AddTree(parent_tree, true);
  combiner.AddTree(child_tree, false);
  combiner.Combine();

  const AXTreeUpdate& combined = combiner.combined();

  ASSERT_EQ(6U, combined.nodes.size());
  EXPECT_EQ(6, combined.tree_data.focus_id);
}

}  // namespace ui
