// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/tree_node_model.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace ui {

class TreeNodeModelTest : public testing::Test, public TreeModelObserver {
 public:
  TreeNodeModelTest()
      : added_count_(0),
        removed_count_(0),
        changed_count_(0) {}
  ~TreeNodeModelTest() override {}

 protected:
  std::string GetObserverCountStateAndClear() {
    std::string result(base::StringPrintf("added=%d removed=%d changed=%d",
        added_count_, removed_count_, changed_count_));
    added_count_ = removed_count_ = changed_count_ = 0;
    return result;
  }

 private:
  // Overridden from TreeModelObserver:
  void TreeNodesAdded(TreeModel* model,
                      TreeModelNode* parent,
                      int start,
                      int count) override {
    added_count_++;
  }
  void TreeNodesRemoved(TreeModel* model,
                        TreeModelNode* parent,
                        int start,
                        int count) override {
    removed_count_++;
  }
  void TreeNodeChanged(TreeModel* model, TreeModelNode* node) override {
    changed_count_++;
  }

  int added_count_;
  int removed_count_;
  int changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TreeNodeModelTest);
};

typedef TreeNodeWithValue<int> TestNode;

// Verifies if the model is properly adding a new node in the tree and
// notifying the observers.
// The tree looks like this:
// root
// +-- child1
//     +-- foo1
//     +-- foo2
// +-- child2
TEST_F(TreeNodeModelTest, AddNode) {
  TestNode* root = new TestNode;
  TreeNodeModel<TestNode > model(root);
  model.AddObserver(this);

  TestNode* child1 = new TestNode;
  model.Add(root, child1, 0);

  EXPECT_EQ("added=1 removed=0 changed=0", GetObserverCountStateAndClear());

  for (int i = 0; i < 2; ++i)
    child1->Add(new TestNode, i);

  TestNode* child2 = new TestNode;
  model.Add(root, child2, 1);

  EXPECT_EQ("added=1 removed=0 changed=0", GetObserverCountStateAndClear());

  EXPECT_EQ(2, root->child_count());
  EXPECT_EQ(2, child1->child_count());
  EXPECT_EQ(0, child2->child_count());
}

// Verifies if the model is properly removing a node from the tree
// and notifying the observers.
TEST_F(TreeNodeModelTest, RemoveNode) {
  TestNode* root = new TestNode;
  TreeNodeModel<TestNode > model(root);
  model.AddObserver(this);

  TestNode* child1 = new TestNode;
  root->Add(child1, 0);

  EXPECT_EQ(1, model.GetChildCount(root));

  // Now remove |child1| from |root| and release the memory.
  delete model.Remove(root, child1);

  EXPECT_EQ("added=0 removed=1 changed=0", GetObserverCountStateAndClear());

  EXPECT_EQ(0, model.GetChildCount(root));
}

// Verifies if the nodes added under the root are all deleted when calling
// RemoveAll. Note that is responsability of the caller to free the memory
// of the nodes removed after RemoveAll is called.
// The tree looks like this:
// root
// +-- child1
//     +-- foo
//         +-- bar0
//         +-- bar1
//         +-- bar2
// +-- child2
// +-- child3
TEST_F(TreeNodeModelTest, RemoveAllNodes) {
  TestNode root;

  TestNode child1;
  TestNode child2;
  TestNode child3;

  root.Add(&child1, 0);
  root.Add(&child2, 1);
  root.Add(&child3, 2);

  TestNode* foo = new TestNode;
  child1.Add(foo, 0);

  // Add some nodes to |foo|.
  for (int i = 0; i < 3; ++i)
    foo->Add(new TestNode, i);

  EXPECT_EQ(3, root.child_count());
  EXPECT_EQ(1, child1.child_count());
  EXPECT_EQ(3, foo->child_count());

  // Now remove the child nodes from root.
  root.RemoveAll();

  EXPECT_EQ(0, root.child_count());
  EXPECT_TRUE(root.empty());

  EXPECT_EQ(1, child1.child_count());
  EXPECT_EQ(3, foo->child_count());
}

// Verifies if GetIndexOf() returns the correct index for the specified node.
// The tree looks like this:
// root
// +-- child1
//     +-- foo1
// +-- child2
TEST_F(TreeNodeModelTest, GetIndexOf) {
  TestNode root;

  TestNode* child1 = new TestNode;
  root.Add(child1, 0);

  TestNode* child2 = new TestNode;
  root.Add(child2, 1);

  TestNode* foo1 = new TestNode;
  child1->Add(foo1, 0);

  EXPECT_EQ(-1, root.GetIndexOf(&root));
  EXPECT_EQ(0, root.GetIndexOf(child1));
  EXPECT_EQ(1, root.GetIndexOf(child2));
  EXPECT_EQ(-1, root.GetIndexOf(foo1));

  EXPECT_EQ(-1, child1->GetIndexOf(&root));
  EXPECT_EQ(-1, child1->GetIndexOf(child1));
  EXPECT_EQ(-1, child1->GetIndexOf(child2));
  EXPECT_EQ(0, child1->GetIndexOf(foo1));

  EXPECT_EQ(-1, child2->GetIndexOf(&root));
  EXPECT_EQ(-1, child2->GetIndexOf(child2));
  EXPECT_EQ(-1, child2->GetIndexOf(child1));
  EXPECT_EQ(-1, child2->GetIndexOf(foo1));
}

// Verifies whether a specified node has or not an ancestor.
// The tree looks like this:
// root
// +-- child1
//     +-- foo1
// +-- child2
TEST_F(TreeNodeModelTest, HasAncestor) {
  TestNode root;
  TestNode* child1 = new TestNode;
  TestNode* child2 = new TestNode;

  root.Add(child1, 0);
  root.Add(child2, 1);

  TestNode* foo1 = new TestNode;
  child1->Add(foo1, 0);

  EXPECT_TRUE(root.HasAncestor(&root));
  EXPECT_FALSE(root.HasAncestor(child1));
  EXPECT_FALSE(root.HasAncestor(child2));
  EXPECT_FALSE(root.HasAncestor(foo1));

  EXPECT_TRUE(child1->HasAncestor(child1));
  EXPECT_TRUE(child1->HasAncestor(&root));
  EXPECT_FALSE(child1->HasAncestor(child2));
  EXPECT_FALSE(child1->HasAncestor(foo1));

  EXPECT_TRUE(child2->HasAncestor(child2));
  EXPECT_TRUE(child2->HasAncestor(&root));
  EXPECT_FALSE(child2->HasAncestor(child1));
  EXPECT_FALSE(child2->HasAncestor(foo1));

  EXPECT_TRUE(foo1->HasAncestor(foo1));
  EXPECT_TRUE(foo1->HasAncestor(child1));
  EXPECT_TRUE(foo1->HasAncestor(&root));
  EXPECT_FALSE(foo1->HasAncestor(child2));
}

// Verifies if GetTotalNodeCount returns the correct number of nodes from the
// node specifed. The count should include the node itself.
// The tree looks like this:
// root
// +-- child1
//     +-- child2
//         +-- child3
// +-- foo1
//     +-- foo2
//         +-- foo3
//     +-- foo4
// +-- bar1
//
// The TotalNodeCount of root is:            9
// The TotalNodeCount of child1 is:          3
// The TotalNodeCount of child2 and foo2 is: 2
// The TotalNodeCount of bar1 is:            1
// And so on...
TEST_F(TreeNodeModelTest, GetTotalNodeCount) {
  TestNode root;

  TestNode* child1 = new TestNode;
  TestNode* child2 = new TestNode;
  TestNode* child3 = new TestNode;

  root.Add(child1, 0);
  child1->Add(child2, 0);
  child2->Add(child3, 0);

  TestNode* foo1 = new TestNode;
  TestNode* foo2 = new TestNode;
  TestNode* foo3 = new TestNode;
  TestNode* foo4 = new TestNode;

  root.Add(foo1, 1);
  foo1->Add(foo2, 0);
  foo2->Add(foo3, 0);
  foo1->Add(foo4, 1);

  TestNode* bar1 = new TestNode;

  root.Add(bar1, 2);

  EXPECT_EQ(9, root.GetTotalNodeCount());
  EXPECT_EQ(3, child1->GetTotalNodeCount());
  EXPECT_EQ(2, child2->GetTotalNodeCount());
  EXPECT_EQ(2, foo2->GetTotalNodeCount());
  EXPECT_EQ(1, bar1->GetTotalNodeCount());
}

// Makes sure that we are notified when the node is renamed,
// also makes sure the node is properly renamed.
TEST_F(TreeNodeModelTest, SetTitle) {
  TestNode* root = new TestNode(ASCIIToUTF16("root"), 0);
  TreeNodeModel<TestNode > model(root);
  model.AddObserver(this);

  const base::string16 title(ASCIIToUTF16("root2"));
  model.SetTitle(root, title);
  EXPECT_EQ("added=0 removed=0 changed=1", GetObserverCountStateAndClear());
  EXPECT_EQ(title, root->GetTitle());
}

TEST_F(TreeNodeModelTest, BasicOperations) {
  TestNode root;
  EXPECT_EQ(0, root.child_count());

  TestNode* child1 = new TestNode;
  root.Add(child1, root.child_count());
  EXPECT_EQ(1, root.child_count());
  EXPECT_EQ(&root, child1->parent());

  TestNode* child2 = new TestNode;
  root.Add(child2, root.child_count());
  EXPECT_EQ(2, root.child_count());
  EXPECT_EQ(child1->parent(), child2->parent());

  std::unique_ptr<TestNode> c2(root.Remove(child2));
  EXPECT_EQ(1, root.child_count());
  EXPECT_EQ(NULL, child2->parent());

  std::unique_ptr<TestNode> c1(root.Remove(child1));
  EXPECT_EQ(0, root.child_count());
}

TEST_F(TreeNodeModelTest, IsRoot) {
  TestNode root;
  EXPECT_TRUE(root.is_root());

  TestNode* child1 = new TestNode;
  root.Add(child1, root.child_count());
  EXPECT_FALSE(child1->is_root());
}

}  // namespace ui
