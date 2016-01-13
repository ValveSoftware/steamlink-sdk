// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace ui {

namespace {

class FakeAXTreeDelegate : public AXTreeDelegate {
 public:
  virtual void OnNodeWillBeDeleted(AXNode* node) OVERRIDE {
    deleted_ids_.push_back(node->id());
  }

  virtual void OnNodeCreated(AXNode* node) OVERRIDE {
    created_ids_.push_back(node->id());
  }

  virtual void OnNodeChanged(AXNode* node) OVERRIDE {
    changed_ids_.push_back(node->id());
  }

  virtual void OnNodeCreationFinished(AXNode* node) OVERRIDE {
    creation_finished_ids_.push_back(node->id());
  }

  virtual void OnNodeChangeFinished(AXNode* node) OVERRIDE {
    change_finished_ids_.push_back(node->id());
  }

  virtual void OnRootChanged(AXNode* new_root) OVERRIDE {
    new_root_ids_.push_back(new_root->id());
  }

  const std::vector<int32>& deleted_ids() { return deleted_ids_; }
  const std::vector<int32>& created_ids() { return created_ids_; }
  const std::vector<int32>& creation_finished_ids() {
    return creation_finished_ids_;
  }
  const std::vector<int32>& new_root_ids() { return new_root_ids_; }

 private:
  std::vector<int32> deleted_ids_;
  std::vector<int32> created_ids_;
  std::vector<int32> creation_finished_ids_;
  std::vector<int32> changed_ids_;
  std::vector<int32> change_finished_ids_;
  std::vector<int32> new_root_ids_;
};

}  // namespace

TEST(AXTreeTest, SerializeSimpleAXTree) {
  AXNodeData root;
  root.id = 1;
  root.role = AX_ROLE_ROOT_WEB_AREA;
  root.state = (1 << AX_STATE_FOCUSABLE) | (1 << AX_STATE_FOCUSED);
  root.location = gfx::Rect(0, 0, 800, 600);
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  AXNodeData button;
  button.id = 2;
  button.role = AX_ROLE_BUTTON;
  button.state = 0;
  button.location = gfx::Rect(20, 20, 200, 30);

  AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.role = AX_ROLE_CHECK_BOX;
  checkbox.state = 0;
  checkbox.location = gfx::Rect(20, 50, 200, 30);

  AXTreeUpdate initial_state;
  initial_state.nodes.push_back(root);
  initial_state.nodes.push_back(button);
  initial_state.nodes.push_back(checkbox);
  AXSerializableTree src_tree(initial_state);

  scoped_ptr<AXTreeSource<const AXNode*> > tree_source(
      src_tree.CreateTreeSource());
  AXTreeSerializer<const AXNode*> serializer(tree_source.get());
  AXTreeUpdate update;
  serializer.SerializeChanges(src_tree.GetRoot(), &update);

  AXTree dst_tree;
  ASSERT_TRUE(dst_tree.Unserialize(update));

  const AXNode* root_node = dst_tree.GetRoot();
  ASSERT_TRUE(root_node != NULL);
  EXPECT_EQ(root.id, root_node->id());
  EXPECT_EQ(root.role, root_node->data().role);

  ASSERT_EQ(2, root_node->child_count());

  const AXNode* button_node = root_node->ChildAtIndex(0);
  EXPECT_EQ(button.id, button_node->id());
  EXPECT_EQ(button.role, button_node->data().role);

  const AXNode* checkbox_node = root_node->ChildAtIndex(1);
  EXPECT_EQ(checkbox.id, checkbox_node->id());
  EXPECT_EQ(checkbox.role, checkbox_node->data().role);

  EXPECT_EQ(
      "id=1 rootWebArea FOCUSABLE FOCUSED (0, 0)-(800, 600) child_ids=2,3\n"
      "  id=2 button (20, 20)-(200, 30)\n"
      "  id=3 checkBox (20, 50)-(200, 30)\n",
      dst_tree.ToString());
}

TEST(AXTreeTest, SerializeAXTreeUpdate) {
  AXNodeData list;
  list.id = 3;
  list.role = AX_ROLE_LIST;
  list.state = 0;
  list.child_ids.push_back(4);
  list.child_ids.push_back(5);
  list.child_ids.push_back(6);

  AXNodeData list_item_2;
  list_item_2.id = 5;
  list_item_2.role = AX_ROLE_LIST_ITEM;
  list_item_2.state = 0;

  AXNodeData list_item_3;
  list_item_3.id = 6;
  list_item_3.role = AX_ROLE_LIST_ITEM;
  list_item_3.state = 0;

  AXNodeData button;
  button.id = 7;
  button.role = AX_ROLE_BUTTON;
  button.state = 0;

  AXTreeUpdate update;
  update.nodes.push_back(list);
  update.nodes.push_back(list_item_2);
  update.nodes.push_back(list_item_3);
  update.nodes.push_back(button);

  EXPECT_EQ(
      "id=3 list (0, 0)-(0, 0) child_ids=4,5,6\n"
      "  id=5 listItem (0, 0)-(0, 0)\n"
      "  id=6 listItem (0, 0)-(0, 0)\n"
      "id=7 button (0, 0)-(0, 0)\n",
      update.ToString());
}

TEST(AXTreeTest, DeleteUnknownSubtreeFails) {
  AXNodeData root;
  root.id = 1;
  root.role = AX_ROLE_ROOT_WEB_AREA;

  AXTreeUpdate initial_state;
  initial_state.nodes.push_back(root);
  AXTree tree(initial_state);

  // This should fail because we're asking it to delete
  // a subtree rooted at id=2, which doesn't exist.
  AXTreeUpdate update;
  update.node_id_to_clear = 2;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  update.nodes[0].id = AX_ROLE_ROOT_WEB_AREA;
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Bad node_id_to_clear: 2", tree.error());
}

TEST(AXTreeTest, LeaveOrphanedDeletedSubtreeFails) {
  AXTreeUpdate initial_state;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  AXTree tree(initial_state);

  // This should fail because we delete a subtree rooted at id=2
  // but never update it.
  AXTreeUpdate update;
  update.node_id_to_clear = 2;
  update.nodes.resize(1);
  update.nodes[0].id = 3;
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Nodes left pending by the update: 2", tree.error());
}

TEST(AXTreeTest, LeaveOrphanedNewChildFails) {
  AXTreeUpdate initial_state;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  AXTree tree(initial_state);

  // This should fail because we add a new child to the root node
  // but never update it.
  AXTreeUpdate update;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  update.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  update.nodes[0].child_ids.push_back(2);
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Nodes left pending by the update: 2", tree.error());
}

TEST(AXTreeTest, DuplicateChildIdFails) {
  AXTreeUpdate initial_state;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  AXTree tree(initial_state);

  // This should fail because a child id appears twice.
  AXTreeUpdate update;
  update.nodes.resize(2);
  update.nodes[0].id = 1;
  update.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  update.nodes[0].child_ids.push_back(2);
  update.nodes[0].child_ids.push_back(2);
  update.nodes[1].id = 2;
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Node 1 has duplicate child id 2", tree.error());
}

TEST(AXTreeTest, InvalidReparentingFails) {
  AXTreeUpdate initial_state;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;

  AXTree tree(initial_state);

  // This should fail because node 3 is reparented from node 2 to node 1
  // without deleting node 1's subtree first.
  AXTreeUpdate update;
  update.nodes.resize(3);
  update.nodes[0].id = 1;
  update.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  update.nodes[0].child_ids.push_back(3);
  update.nodes[0].child_ids.push_back(2);
  update.nodes[1].id = 2;
  update.nodes[2].id = 3;
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Node 3 reparented from 2 to 1", tree.error());
}

TEST(AXTreeTest, TreeDelegateIsCalled) {
  AXTreeUpdate initial_state;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;

  AXTree tree(initial_state);
  AXTreeUpdate update;
  update.node_id_to_clear = 1;
  update.nodes.resize(2);
  update.nodes[0].id = 2;
  update.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  update.nodes[0].child_ids.push_back(3);
  update.nodes[1].id = 3;

  FakeAXTreeDelegate fake_delegate;
  tree.SetDelegate(&fake_delegate);

  EXPECT_TRUE(tree.Unserialize(update));

  ASSERT_EQ(1U, fake_delegate.deleted_ids().size());
  EXPECT_EQ(1, fake_delegate.deleted_ids()[0]);

  ASSERT_EQ(2U, fake_delegate.created_ids().size());
  EXPECT_EQ(2, fake_delegate.created_ids()[0]);
  EXPECT_EQ(3, fake_delegate.created_ids()[1]);

  ASSERT_EQ(2U, fake_delegate.creation_finished_ids().size());
  EXPECT_EQ(2, fake_delegate.creation_finished_ids()[0]);
  EXPECT_EQ(3, fake_delegate.creation_finished_ids()[1]);

  ASSERT_EQ(1U, fake_delegate.new_root_ids().size());
  EXPECT_EQ(2, fake_delegate.new_root_ids()[0]);

  tree.SetDelegate(NULL);
}

}  // namespace ui
