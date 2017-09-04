// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

using TestPositionType = AXPosition<AXNodePosition, AXNode>;

namespace {

class AXPositionTest : public testing::Test {
 public:
  const char* TEXT_VALUE = "Line 1\nLine 2";

  AXPositionTest();
  ~AXPositionTest() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  AXNodeData root_;
  AXNodeData button_;
  AXNodeData check_box_;
  AXNodeData text_field_;
  AXNodeData static_text1_;
  AXNodeData line_break_;
  AXNodeData static_text2_;
  AXNodeData inline_box1_;
  AXNodeData inline_box2_;

  AXTree tree_;
  DISALLOW_COPY_AND_ASSIGN(AXPositionTest);
};

AXPositionTest::AXPositionTest() {}

AXPositionTest::~AXPositionTest() {}

void AXPositionTest::SetUp() {
  std::vector<int32_t> line_start_offsets{0, 6};
  std::vector<int32_t> word_starts{0, 5};
  std::vector<int32_t> word_ends{3, 6};

  root_.id = 1;
  button_.id = 2;
  check_box_.id = 3;
  text_field_.id = 4;
  static_text1_.id = 5;
  inline_box1_.id = 6;
  line_break_.id = 7;
  static_text2_.id = 8;
  inline_box2_.id = 9;

  root_.role = AX_ROLE_DIALOG;
  root_.state = 1 << AX_STATE_FOCUSABLE;
  root_.location = gfx::RectF(0, 0, 800, 600);

  button_.role = AX_ROLE_BUTTON;
  button_.state = 1 << AX_STATE_HASPOPUP;
  button_.SetName("Sample button_");
  button_.location = gfx::RectF(20, 20, 200, 30);
  root_.child_ids.push_back(button_.id);

  check_box_.role = AX_ROLE_CHECK_BOX;
  check_box_.state = 1 << AX_STATE_CHECKED;
  check_box_.SetName("Sample check box");
  check_box_.location = gfx::RectF(20, 50, 200, 30);
  root_.child_ids.push_back(check_box_.id);

  text_field_.role = AX_ROLE_TEXT_FIELD;
  text_field_.state = 1 << AX_STATE_EDITABLE;
  text_field_.SetValue(TEXT_VALUE);
  text_field_.AddIntListAttribute(AX_ATTR_CACHED_LINE_STARTS,
                                  line_start_offsets);
  text_field_.child_ids.push_back(static_text1_.id);
  text_field_.child_ids.push_back(line_break_.id);
  text_field_.child_ids.push_back(static_text2_.id);
  root_.child_ids.push_back(text_field_.id);

  static_text1_.role = AX_ROLE_STATIC_TEXT;
  static_text1_.state = 1 << AX_STATE_EDITABLE;
  static_text1_.SetName("Line 1");
  static_text1_.child_ids.push_back(inline_box1_.id);

  inline_box1_.role = AX_ROLE_INLINE_TEXT_BOX;
  inline_box1_.state = 1 << AX_STATE_EDITABLE;
  inline_box1_.SetName("Line 1");
  inline_box1_.AddIntListAttribute(AX_ATTR_WORD_STARTS, word_starts);
  inline_box1_.AddIntListAttribute(AX_ATTR_WORD_ENDS, word_ends);

  line_break_.role = AX_ROLE_LINE_BREAK;
  line_break_.state = 1 << AX_STATE_EDITABLE;
  line_break_.SetName("\n");

  static_text2_.role = AX_ROLE_STATIC_TEXT;
  static_text2_.state = 1 << AX_STATE_EDITABLE;
  static_text2_.SetName("Line 2");
  static_text2_.child_ids.push_back(inline_box2_.id);

  inline_box2_.role = AX_ROLE_INLINE_TEXT_BOX;
  inline_box2_.state = 1 << AX_STATE_EDITABLE;
  inline_box2_.SetName("Line 2");
  inline_box2_.AddIntListAttribute(AX_ATTR_WORD_STARTS, word_starts);
  inline_box2_.AddIntListAttribute(AX_ATTR_WORD_ENDS, word_ends);

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.push_back(root_);
  initial_state.nodes.push_back(button_);
  initial_state.nodes.push_back(check_box_);
  initial_state.nodes.push_back(text_field_);
  initial_state.nodes.push_back(static_text1_);
  initial_state.nodes.push_back(inline_box1_);
  initial_state.nodes.push_back(line_break_);
  initial_state.nodes.push_back(static_text2_);
  initial_state.nodes.push_back(inline_box2_);
  initial_state.has_tree_data = true;
  initial_state.tree_data.tree_id = 0;
  initial_state.tree_data.title = "Dialog title";
  AXSerializableTree src_tree(initial_state);

  std::unique_ptr<AXTreeSource<const AXNode*, AXNodeData, AXTreeData>>
      tree_source(src_tree.CreateTreeSource());
  AXTreeSerializer<const AXNode*, AXNodeData, AXTreeData> serializer(
      tree_source.get());
  AXTreeUpdate update;
  serializer.SerializeChanges(src_tree.root(), &update);
  ASSERT_TRUE(tree_.Unserialize(update));
  AXNodePosition::SetTreeForTesting(&tree_);
}

void AXPositionTest::TearDown() {
  AXNodePosition::SetTreeForTesting(nullptr);
}

}  // namespace

TEST_F(AXPositionTest, AtStartOfAnchorWithNullPosition) {
  std::unique_ptr<TestPositionType> null_position(
      AXNodePosition::CreateNullPosition());
  ASSERT_NE(nullptr, null_position);
  EXPECT_FALSE(null_position->AtStartOfAnchor());
}

TEST_F(AXPositionTest, AtStartOfAnchorWithTreePosition) {
  std::unique_ptr<TestPositionType> tree_position(
      AXNodePosition::CreateTreePosition(tree_.data().tree_id, root_.id,
                                         0 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  EXPECT_TRUE(tree_position->AtStartOfAnchor());

  tree_position.reset(AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtStartOfAnchor());

  tree_position.reset(AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtStartOfAnchor());
}

TEST_F(AXPositionTest, AtStartOfAnchorWithTextPosition) {
  std::unique_ptr<TestPositionType> text_position(
      AXNodePosition::CreateTextPosition(tree_.data().tree_id, inline_box1_.id,
                                         0 /* text_offset */,
                                         AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  EXPECT_TRUE(text_position->AtStartOfAnchor());
  text_position.reset(AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 1 /* text_offset */,
      AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  EXPECT_FALSE(text_position->AtStartOfAnchor());

  text_position.reset(AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  EXPECT_FALSE(text_position->AtStartOfAnchor());
}

TEST_F(AXPositionTest, AtEndOfAnchorWithNullPosition) {
  std::unique_ptr<TestPositionType> null_position(
      AXNodePosition::CreateNullPosition());
  ASSERT_NE(nullptr, null_position);
  EXPECT_FALSE(null_position->AtEndOfAnchor());
}

TEST_F(AXPositionTest, AtEndOfAnchorWithTreePosition) {
  std::unique_ptr<TestPositionType> tree_position(
      AXNodePosition::CreateTreePosition(tree_.data().tree_id, root_.id,
                                         3 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  EXPECT_TRUE(tree_position->AtEndOfAnchor());

  tree_position.reset(AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 2 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtEndOfAnchor());

  tree_position.reset(AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtEndOfAnchor());
}

TEST_F(AXPositionTest, AtEndOfAnchorWithTextPosition) {
  std::unique_ptr<TestPositionType> text_position(
      AXNodePosition::CreateTextPosition(tree_.data().tree_id, inline_box1_.id,
                                         6 /* text_offset */,
                                         AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  EXPECT_TRUE(text_position->AtEndOfAnchor());

  text_position.reset(AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  EXPECT_FALSE(text_position->AtEndOfAnchor());

  text_position.reset(AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  EXPECT_FALSE(text_position->AtEndOfAnchor());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfAnchorWithNullPosition) {
  std::unique_ptr<TestPositionType> null_position(
      AXNodePosition::CreateNullPosition());
  ASSERT_NE(nullptr, null_position);
  std::unique_ptr<TestPositionType> test_position(
      null_position->CreatePositionAtStartOfAnchor());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfAnchorWithTreePosition) {
  std::unique_ptr<TestPositionType> tree_position(
      AXNodePosition::CreateTreePosition(tree_.data().tree_id, root_.id,
                                         0 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  std::unique_ptr<TestPositionType> test_position(
      tree_position->CreatePositionAtStartOfAnchor());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  tree_position.reset(AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  test_position.reset(tree_position->CreatePositionAtStartOfAnchor());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfAnchorWithTextPosition) {
  std::unique_ptr<TestPositionType> text_position(
      AXNodePosition::CreateTextPosition(tree_.data().tree_id, inline_box1_.id,
                                         0 /* text_offset */,
                                         AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  std::unique_ptr<TestPositionType> test_position(
      text_position->CreatePositionAtStartOfAnchor());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position.reset(AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 1 /* text_offset */,
      AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  test_position.reset(text_position->CreatePositionAtStartOfAnchor());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfAnchorWithNullPosition) {
  std::unique_ptr<TestPositionType> null_position(
      AXNodePosition::CreateNullPosition());
  ASSERT_NE(nullptr, null_position);
  std::unique_ptr<TestPositionType> test_position(
      null_position->CreatePositionAtEndOfAnchor());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfAnchorWithTreePosition) {
  std::unique_ptr<TestPositionType> tree_position(
      AXNodePosition::CreateTreePosition(tree_.data().tree_id, root_.id,
                                         3 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  std::unique_ptr<TestPositionType> test_position(
      tree_position->CreatePositionAtEndOfAnchor());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->child_index());

  tree_position.reset(AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  test_position.reset(tree_position->CreatePositionAtEndOfAnchor());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->child_index());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfAnchorWithTextPosition) {
  std::unique_ptr<TestPositionType> text_position(
      AXNodePosition::CreateTextPosition(tree_.data().tree_id, inline_box1_.id,
                                         6 /* text_offset */,
                                         AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  std::unique_ptr<TestPositionType> test_position(
      text_position->CreatePositionAtEndOfAnchor());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  text_position.reset(AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  test_position.reset(text_position->CreatePositionAtEndOfAnchor());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
}

TEST_F(AXPositionTest, CreateChildPositionAtWithNullPosition) {
  std::unique_ptr<TestPositionType> null_position(
      AXNodePosition::CreateNullPosition());
  ASSERT_NE(nullptr, null_position);
  std::unique_ptr<TestPositionType> test_position(
      null_position->CreateChildPositionAt(0));
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateChildPositionAtWithTreePosition) {
  std::unique_ptr<TestPositionType> tree_position(
      AXNodePosition::CreateTreePosition(tree_.data().tree_id, root_.id,
                                         2 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  std::unique_ptr<TestPositionType> test_position(
      tree_position->CreateChildPositionAt(1));
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  tree_position.reset(AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, button_.id, 0 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  test_position.reset(tree_position->CreateChildPositionAt(0));
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateChildPositionAtWithTextPosition) {
  std::unique_ptr<TestPositionType> text_position(
      AXNodePosition::CreateTextPosition(tree_.data().tree_id, static_text1_.id,
                                         5 /* text_offset */,
                                         AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  std::unique_ptr<TestPositionType> test_position(
      text_position->CreateChildPositionAt(0));
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position.reset(AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text2_.id, 4 /* text_offset */,
      AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  test_position.reset(text_position->CreateChildPositionAt(1));
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateParentPositionWithNullPosition) {
  std::unique_ptr<TestPositionType> null_position(
      AXNodePosition::CreateNullPosition());
  ASSERT_NE(nullptr, null_position);
  std::unique_ptr<TestPositionType> test_position(
      null_position->CreateParentPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateParentPositionWithTreePosition) {
  std::unique_ptr<TestPositionType> tree_position(
      AXNodePosition::CreateTreePosition(tree_.data().tree_id, check_box_.id,
                                         0 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  std::unique_ptr<TestPositionType> test_position(
      tree_position->CreateParentPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  tree_position.reset(AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */));
  ASSERT_NE(nullptr, tree_position);
  test_position.reset(tree_position->CreateParentPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateParentPositionWithTextPosition) {
  std::unique_ptr<TestPositionType> text_position(
      AXNodePosition::CreateTextPosition(tree_.data().tree_id, inline_box1_.id,
                                         5 /* text_offset */,
                                         AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  std::unique_ptr<TestPositionType> test_position(
      text_position->CreateParentPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(static_text1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
}

TEST_F(AXPositionTest, CreateNextCharacterPositionWithNullPosition) {
  std::unique_ptr<TestPositionType> null_position(
      AXNodePosition::CreateNullPosition());
  ASSERT_NE(nullptr, null_position);
  std::unique_ptr<TestPositionType> test_position(
      null_position->CreateNextCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateNextCharacterPositionWithTextPosition) {
  std::unique_ptr<TestPositionType> text_position(
      AXNodePosition::CreateTextPosition(tree_.data().tree_id, inline_box1_.id,
                                         4 /* text_offset */,
                                         AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  std::unique_ptr<TestPositionType> test_position(
      text_position->CreateNextCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());

  text_position.reset(AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  test_position.reset(text_position->CreateNextCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position.reset(test_position->CreateNextCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position.reset(test_position->CreateNextCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  text_position.reset(AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 15 /* text_offset */,
      AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  test_position.reset(text_position->CreateNextCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
}

TEST_F(AXPositionTest, CreatePreviousCharacterPositionWithNullPosition) {
  std::unique_ptr<TestPositionType> null_position(
      AXNodePosition::CreateNullPosition());
  ASSERT_NE(nullptr, null_position);
  std::unique_ptr<TestPositionType> test_position(
      null_position->CreatePreviousCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePreviousCharacterPositionWithTextPosition) {
  std::unique_ptr<TestPositionType> text_position(
      AXNodePosition::CreateTextPosition(tree_.data().tree_id, inline_box2_.id,
                                         5 /* text_offset */,
                                         AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  std::unique_ptr<TestPositionType> test_position(
      text_position->CreatePreviousCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(4, test_position->text_offset());

  text_position.reset(AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  test_position.reset(text_position->CreatePreviousCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position.reset(test_position->CreatePreviousCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());

  test_position.reset(test_position->CreatePreviousCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(4, test_position->text_offset());

  text_position.reset(AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      AX_TEXT_AFFINITY_UPSTREAM));
  ASSERT_NE(nullptr, text_position);
  test_position.reset(text_position->CreatePreviousCharacterPosition());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(15, test_position->text_offset());
}

}  // namespace ui
