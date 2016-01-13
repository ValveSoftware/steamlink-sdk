// Copyright 2014 The Chromium Authors. All rights reserved.
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

// A function to turn a tree into a string, capturing only the node ids
// and their relationship to one another.
//
// The string format is kind of like an S-expression, with each expression
// being either a node id, or a node id followed by a subexpression
// representing its children.
//
// Examples:
//
// (1) is a tree with a single node with id 1.
// (1 (2 3)) is a tree with 1 as the root, and 2 and 3 as its children.
// (1 (2 (3))) has 1 as the root, 2 as its child, and then 3 as the child of 2.
void TreeToStringHelper(const AXNode* node, std::string* out_result) {
  *out_result += base::IntToString(node->id());
  if (node->child_count() != 0) {
    *out_result += " (";
    for (int i = 0; i < node->child_count(); ++i) {
      if (i != 0)
        *out_result += " ";
      TreeToStringHelper(node->ChildAtIndex(i), out_result);
    }
    *out_result += ")";
  }
}

std::string TreeToString(const AXTree& tree) {
  std::string result;
  TreeToStringHelper(tree.GetRoot(), &result);
  return "(" + result + ")";
}

}  // anonymous namespace

// A class to create all possible trees with <n> nodes and the ids [1...n].
//
// There are two parts to the algorithm:
//
// The tree structure is formed as follows: without loss of generality,
// the first node becomes the root and the second node becomes its
// child. Thereafter, choose every possible parent for every other node.
//
// So for node i in (3...n), there are (i - 1) possible choices for its
// parent, for a total of (n-1)! (n minus 1 factorial) possible trees.
//
// The second part is the assignment of ids to the nodes in the tree.
// There are exactly n! (n factorial) permutations of the sequence 1...n,
// and each of these is assigned to every node in every possible tree.
//
// The total number of trees returned for a given <n>, then, is
// n! * (n-1)!
//
// n = 2: 2 trees
// n = 3: 12 trees
// n = 4: 144 trees
// n = 5: 2880 trees
//
// This grows really fast! Luckily it's very unlikely that there'd be
// bugs that affect trees with >4 nodes that wouldn't affect a smaller tree
// too.
class TreeGenerator {
 public:
  TreeGenerator(int node_count)
      : node_count_(node_count),
        unique_tree_count_(1) {
    // (n-1)! for the possible trees.
    for (int i = 2; i < node_count_; i++)
      unique_tree_count_ *= i;
    // n! for the permutations of ids.
    for (int i = 2; i <= node_count_; i++)
      unique_tree_count_ *= i;
  }

  int UniqueTreeCount() {
    return unique_tree_count_;
  }

  void BuildUniqueTree(int tree_index, AXTree* out_tree) {
    std::vector<int> indices;
    std::vector<int> permuted;
    CHECK(tree_index <= unique_tree_count_);

    // Use the first few bits of |tree_index| to permute
    // the indices.
    for (int i = 0; i < node_count_; i++)
      indices.push_back(i + 1);
    for (int i = 0; i < node_count_; i++) {
      int p = (node_count_ - i);
      int index = tree_index % p;
      tree_index /= p;
      permuted.push_back(indices[index]);
      indices.erase(indices.begin() + index);
    }

    // Build an AXTreeUpdate. The first two nodes of the tree always
    // go in the same place.
    AXTreeUpdate update;
    update.nodes.resize(node_count_);
    update.nodes[0].id = permuted[0];
    update.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
    update.nodes[0].child_ids.push_back(permuted[1]);
    update.nodes[1].id = permuted[1];

    // The remaining nodes are assigned based on their parent
    // selected from the next bits from |tree_index|.
    for (int i = 2; i < node_count_; i++) {
      update.nodes[i].id = permuted[i];
      int parent_index = (tree_index % i);
      tree_index /= i;
      update.nodes[parent_index].child_ids.push_back(permuted[i]);
    }

    // Unserialize the tree update into the destination tree.
    CHECK(out_tree->Unserialize(update));
  }

 private:
  int node_count_;
  int unique_tree_count_;
};

// Test the TreeGenerator class by building all possible trees with
// 3 nodes and the ids [1...3].
TEST(AXGeneratedTreeTest, TestTreeGenerator) {
  int tree_size = 3;
  TreeGenerator generator(tree_size);
  const char* EXPECTED_TREES[] = {
    "(1 (2 3))",
    "(2 (1 3))",
    "(3 (1 2))",
    "(1 (3 2))",
    "(2 (3 1))",
    "(3 (2 1))",
    "(1 (2 (3)))",
    "(2 (1 (3)))",
    "(3 (1 (2)))",
    "(1 (3 (2)))",
    "(2 (3 (1)))",
    "(3 (2 (1)))",
  };

  int n = generator.UniqueTreeCount();
  ASSERT_EQ(static_cast<int>(arraysize(EXPECTED_TREES)), n);

  for (int i = 0; i < n; i++) {
    AXTree tree;
    generator.BuildUniqueTree(i, &tree);
    std::string str = TreeToString(tree);
    EXPECT_EQ(EXPECTED_TREES[i], str);
  }
}

// Test mutating every possible tree with <n> nodes to every other possible
// tree with <n> nodes, where <n> is 4 in release mode and 3 in debug mode
// (for speed). For each possible combination of trees, we also vary which
// node we serialize first.
//
// For every possible scenario, we check that the AXTreeUpdate is valid,
// that the destination tree can unserialize it and create a valid tree,
// and that after updating all nodes the resulting tree now matches the
// intended tree.
TEST(AXGeneratedTreeTest, SerializeGeneratedTrees) {
  // Do a more exhaustive test in release mode. If you're modifying
  // the algorithm you may want to try even larger tree sizes if you
  // can afford the time.
#ifdef NDEBUG
  int tree_size = 4;
#else
  LOG(WARNING) << "Debug build, only testing trees with 3 nodes and not 4.";
  int tree_size = 3;
#endif

  TreeGenerator generator(tree_size);
  int n = generator.UniqueTreeCount();

  for (int i = 0; i < n; i++) {
    // Build the first tree, tree0.
    AXSerializableTree tree0;
    generator.BuildUniqueTree(i, &tree0);
    SCOPED_TRACE("tree0 is " + TreeToString(tree0));

    for (int j = 0; j < n; j++) {
      // Build the second tree, tree1.
      AXSerializableTree tree1;
      generator.BuildUniqueTree(j, &tree1);
      SCOPED_TRACE("tree1 is " + TreeToString(tree0));

      // Now iterate over which node to update first, |k|.
      for (int k = 0; k < tree_size; k++) {
        SCOPED_TRACE("i=" + base::IntToString(i) +
                     " j=" + base::IntToString(j) +
                     " k=" + base::IntToString(k));

        // Start by serializing tree0 and unserializing it into a new
        // empty tree |dst_tree|.
        scoped_ptr<AXTreeSource<const AXNode*> > tree0_source(
            tree0.CreateTreeSource());
        AXTreeSerializer<const AXNode*> serializer(tree0_source.get());
        AXTreeUpdate update0;
        serializer.SerializeChanges(tree0.GetRoot(), &update0);

        AXTree dst_tree;
        ASSERT_TRUE(dst_tree.Unserialize(update0));

        // At this point, |dst_tree| should now be identical to |tree0|.
        EXPECT_EQ(TreeToString(tree0), TreeToString(dst_tree));

        // Next, pretend that tree0 turned into tree1, and serialize
        // a sequence of updates to |dst_tree| to match.
        scoped_ptr<AXTreeSource<const AXNode*> > tree1_source(
            tree1.CreateTreeSource());
        serializer.ChangeTreeSourceForTesting(tree1_source.get());

        for (int k_index = 0; k_index < tree_size; ++k_index) {
          int id = 1 + (k + k_index) % tree_size;
          AXTreeUpdate update;
          serializer.SerializeChanges(tree1.GetFromId(id), &update);
          ASSERT_TRUE(dst_tree.Unserialize(update));
        }

        // After the sequence of updates, |dst_tree| should now be
        // identical to |tree1|.
        EXPECT_EQ(TreeToString(tree1), TreeToString(dst_tree));
      }
    }
  }
}

}  // namespace ui
