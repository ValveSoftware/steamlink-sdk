// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_priority_forest.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(SpdyPriorityForestTest, AddAndRemoveNodes) {
  SpdyPriorityForest<uint32,int16> forest;
  EXPECT_EQ(0, forest.num_nodes());
  EXPECT_FALSE(forest.NodeExists(1));

  EXPECT_TRUE(forest.AddRootNode(1, 1000));
  EXPECT_EQ(1, forest.num_nodes());
  ASSERT_TRUE(forest.NodeExists(1));
  EXPECT_EQ(1000, forest.GetPriority(1));
  EXPECT_FALSE(forest.NodeExists(5));

  EXPECT_TRUE(forest.AddRootNode(5, 50));
  EXPECT_FALSE(forest.AddRootNode(5, 500));
  EXPECT_EQ(2, forest.num_nodes());
  EXPECT_TRUE(forest.NodeExists(1));
  ASSERT_TRUE(forest.NodeExists(5));
  EXPECT_EQ(50, forest.GetPriority(5));
  EXPECT_FALSE(forest.NodeExists(13));

  EXPECT_TRUE(forest.AddRootNode(13, 130));
  EXPECT_EQ(3, forest.num_nodes());
  EXPECT_TRUE(forest.NodeExists(1));
  EXPECT_TRUE(forest.NodeExists(5));
  ASSERT_TRUE(forest.NodeExists(13));
  EXPECT_EQ(130, forest.GetPriority(13));

  EXPECT_TRUE(forest.RemoveNode(5));
  EXPECT_FALSE(forest.RemoveNode(5));
  EXPECT_EQ(2, forest.num_nodes());
  EXPECT_TRUE(forest.NodeExists(1));
  EXPECT_FALSE(forest.NodeExists(5));
  EXPECT_TRUE(forest.NodeExists(13));

  // The parent node 19 doesn't exist, so this should fail:
  EXPECT_FALSE(forest.AddNonRootNode(7, 19, false));
  // This should succed, creating node 7:
  EXPECT_TRUE(forest.AddNonRootNode(7, 13, false));
  // Now node 7 already exists, so this should fail:
  EXPECT_FALSE(forest.AddNonRootNode(7, 1, false));
  // Node 13 already has a child (7), so this should fail:
  EXPECT_FALSE(forest.AddNonRootNode(17, 13, false));

  ASSERT_TRUE(forest.ValidateInvariantsForTests());
}

TEST(SpdyPriorityForestTest, SetParent) {
  SpdyPriorityForest<uint32,int16> forest;
  forest.AddRootNode(1, 1000);
  forest.AddNonRootNode(2, 1, false);
  forest.AddNonRootNode(3, 2, false);
  forest.AddNonRootNode(5, 3, false);
  forest.AddNonRootNode(7, 5, false);
  forest.AddNonRootNode(9, 7, false);
  forest.AddRootNode(11, 2000);
  forest.AddNonRootNode(13, 11, false);
  // We can't set the parent of a nonexistent node, or set the parent of an
  // existing node to a nonexistent node.
  EXPECT_FALSE(forest.SetParent(99, 13, false));
  EXPECT_FALSE(forest.SetParent(5, 99, false));
  // We can't make a node a child a node that already has a child:
  EXPECT_FALSE(forest.SetParent(13, 7, false));
  EXPECT_FALSE(forest.SetParent(3, 11, false));
  // These would create cycles:
  EXPECT_FALSE(forest.SetParent(11, 13, false));
  EXPECT_FALSE(forest.SetParent(1, 9, false));
  EXPECT_FALSE(forest.SetParent(3, 9, false));
  // But this change is legit:
  EXPECT_EQ(7u, forest.GetChild(5));
  EXPECT_TRUE(forest.SetParent(7, 13, false));
  EXPECT_EQ(0u, forest.GetChild(5));
  EXPECT_EQ(13u, forest.GetParent(7));
  EXPECT_EQ(7u, forest.GetChild(13));
  // So is this change (now that 9 is no longer a descendant of 1):
  EXPECT_TRUE(forest.SetParent(1, 9, false));
  EXPECT_EQ(9u, forest.GetParent(1));
  EXPECT_EQ(1u, forest.GetChild(9));
  // We must allow setting the parent of a node to its same parent (even though
  // that parent of course has a child already), so that we can change
  // orderedness.
  EXPECT_EQ(1u, forest.GetParent(2));
  EXPECT_EQ(2u, forest.GetChild(1));
  EXPECT_FALSE(forest.IsNodeUnordered(2));
  EXPECT_TRUE(forest.SetParent(2, 1, true));
  EXPECT_EQ(1u, forest.GetParent(2));
  EXPECT_EQ(2u, forest.GetChild(1));
  EXPECT_TRUE(forest.IsNodeUnordered(2));

  ASSERT_TRUE(forest.ValidateInvariantsForTests());
}

TEST(SpdyPriorityForestTest, RemoveNodesFromMiddleOfChain) {
  SpdyPriorityForest<uint32,int16> forest;
  forest.AddRootNode(1, 1000);
  forest.AddNonRootNode(2, 1, false);
  forest.AddNonRootNode(3, 2, true);
  forest.AddNonRootNode(5, 3, false);
  forest.AddNonRootNode(7, 5, true);
  forest.AddNonRootNode(9, 7, true);

  // Remove a node from the middle, with unordered links on both sides.  The
  // new merged link should also be unordered.
  EXPECT_TRUE(forest.NodeExists(7));
  EXPECT_EQ(7u, forest.GetChild(5));
  EXPECT_EQ(7u, forest.GetParent(9));
  EXPECT_TRUE(forest.IsNodeUnordered(9));
  EXPECT_TRUE(forest.RemoveNode(7));
  EXPECT_FALSE(forest.NodeExists(7));
  EXPECT_EQ(9u, forest.GetChild(5));
  EXPECT_EQ(5u, forest.GetParent(9));
  EXPECT_TRUE(forest.IsNodeUnordered(9));

  // Remove another node from the middle, with an unordered link on only one
  // side.  The new merged link should be ordered.
  EXPECT_TRUE(forest.NodeExists(2));
  EXPECT_EQ(2u, forest.GetChild(1));
  EXPECT_EQ(2u, forest.GetParent(3));
  EXPECT_FALSE(forest.IsNodeUnordered(2));
  EXPECT_TRUE(forest.IsNodeUnordered(3));
  EXPECT_TRUE(forest.RemoveNode(2));
  EXPECT_FALSE(forest.NodeExists(2));
  EXPECT_EQ(3u, forest.GetChild(1));
  EXPECT_EQ(1u, forest.GetParent(3));
  EXPECT_FALSE(forest.IsNodeUnordered(3));

  // Try removing the root.
  EXPECT_TRUE(forest.NodeExists(1));
  EXPECT_EQ(0u, forest.GetParent(1));
  EXPECT_EQ(1000, forest.GetPriority(1));
  EXPECT_EQ(1u, forest.GetParent(3));
  EXPECT_EQ(0, forest.GetPriority(3));
  EXPECT_TRUE(forest.RemoveNode(1));
  EXPECT_FALSE(forest.NodeExists(1));
  EXPECT_EQ(0u, forest.GetParent(3));
  EXPECT_EQ(1000, forest.GetPriority(3));

  // Now try removing the tail.
  EXPECT_TRUE(forest.NodeExists(9));
  EXPECT_EQ(9u, forest.GetChild(5));
  EXPECT_TRUE(forest.RemoveNode(9));
  EXPECT_FALSE(forest.NodeExists(9));
  EXPECT_EQ(0u, forest.GetChild(5));

  ASSERT_TRUE(forest.ValidateInvariantsForTests());
}

TEST(SpdyPriorityForestTest, MergeOrderedAndUnorderedLinks1) {
  SpdyPriorityForest<uint32,int16> forest;
  forest.AddRootNode(1, 1000);
  forest.AddNonRootNode(2, 1, true);
  forest.AddNonRootNode(3, 2, false);

  EXPECT_EQ(2u, forest.GetChild(1));
  EXPECT_EQ(3u, forest.GetChild(2));
  EXPECT_EQ(1u, forest.GetParent(2));
  EXPECT_EQ(2u, forest.GetParent(3));
  EXPECT_TRUE(forest.IsNodeUnordered(2));
  EXPECT_FALSE(forest.IsNodeUnordered(3));
  EXPECT_TRUE(forest.RemoveNode(2));
  EXPECT_FALSE(forest.NodeExists(2));
  EXPECT_EQ(3u, forest.GetChild(1));
  EXPECT_EQ(1u, forest.GetParent(3));
  EXPECT_FALSE(forest.IsNodeUnordered(3));

  ASSERT_TRUE(forest.ValidateInvariantsForTests());
}

TEST(SpdyPriorityForestTest, MergeOrderedAndUnorderedLinks2) {
  SpdyPriorityForest<uint32,int16> forest;
  forest.AddRootNode(1, 1000);
  forest.AddNonRootNode(2, 1, false);
  forest.AddNonRootNode(3, 2, true);

  EXPECT_EQ(2u, forest.GetChild(1));
  EXPECT_EQ(3u, forest.GetChild(2));
  EXPECT_EQ(1u, forest.GetParent(2));
  EXPECT_EQ(2u, forest.GetParent(3));
  EXPECT_FALSE(forest.IsNodeUnordered(2));
  EXPECT_TRUE(forest.IsNodeUnordered(3));
  EXPECT_TRUE(forest.RemoveNode(2));
  EXPECT_FALSE(forest.NodeExists(2));
  EXPECT_EQ(3u, forest.GetChild(1));
  EXPECT_EQ(1u, forest.GetParent(3));
  EXPECT_FALSE(forest.IsNodeUnordered(3));

  ASSERT_TRUE(forest.ValidateInvariantsForTests());
}

TEST(SpdyPriorityForestTest, WeightedSelectionOfForests) {
  SpdyPriorityForest<uint32,int16> forest;
  forest.AddRootNode(1, 10);
  forest.AddRootNode(3, 20);
  forest.AddRootNode(5, 70);
  EXPECT_EQ(70, forest.GetPriority(5));
  EXPECT_TRUE(forest.SetPriority(5, 40));
  EXPECT_FALSE(forest.SetPriority(7, 80));
  EXPECT_EQ(40, forest.GetPriority(5));
  forest.AddNonRootNode(7, 3, false);
  EXPECT_FALSE(forest.IsMarkedReadyToRead(1));
  EXPECT_TRUE(forest.MarkReadyToRead(1));
  EXPECT_TRUE(forest.IsMarkedReadyToRead(1));
  EXPECT_TRUE(forest.MarkReadyToRead(5));
  EXPECT_TRUE(forest.MarkReadyToRead(7));
  EXPECT_FALSE(forest.MarkReadyToRead(99));

  int counts[8] = {0};
  for (int i = 0; i < 7000; ++i) {
    const uint32 node_id = forest.NextNodeToRead();
    ASSERT_TRUE(node_id == 1 || node_id == 5 || node_id == 7)
        << "node_id is " << node_id;
    ++counts[node_id];
  }

  // In theory, these could fail even if the weighted random selection is
  // implemented correctly, but it's very unlikely.
  EXPECT_GE(counts[1],  800); EXPECT_LE(counts[1], 1200);
  EXPECT_GE(counts[7], 1600); EXPECT_LE(counts[7], 2400);
  EXPECT_GE(counts[5], 3200); EXPECT_LE(counts[5], 4800);

  // If we unmark all but one node, then we know for sure that that node will
  // be selected next.
  EXPECT_TRUE(forest.MarkNoLongerReadyToRead(1));
  EXPECT_TRUE(forest.MarkNoLongerReadyToRead(7));
  EXPECT_FALSE(forest.MarkNoLongerReadyToRead(99));
  EXPECT_EQ(5u, forest.NextNodeToRead());

  ASSERT_TRUE(forest.ValidateInvariantsForTests());
}

TEST(SpdyPriorityForestTest, SelectionBetweenUnorderedNodes) {
  SpdyPriorityForest<uint32,int16> forest;
  forest.AddRootNode(1, 1000);
  forest.AddNonRootNode(2, 1, false);
  forest.AddNonRootNode(3, 2, true);
  forest.AddNonRootNode(4, 3, true);
  forest.AddNonRootNode(5, 4, true);
  forest.AddNonRootNode(6, 5, true);
  forest.AddNonRootNode(7, 6, false);
  EXPECT_FALSE(forest.IsMarkedReadyToWrite(2));
  EXPECT_TRUE(forest.MarkReadyToWrite(2));
  EXPECT_TRUE(forest.IsMarkedReadyToWrite(2));
  EXPECT_TRUE(forest.MarkReadyToWrite(4));
  EXPECT_TRUE(forest.MarkReadyToWrite(6));
  EXPECT_TRUE(forest.MarkReadyToWrite(7));
  EXPECT_FALSE(forest.MarkReadyToWrite(99));

  int counts[8] = {0};
  for (int i = 0; i < 6000; ++i) {
    const uint32 node_id = forest.NextNodeToWrite();
    ASSERT_TRUE(node_id == 2 || node_id == 4 || node_id == 6)
        << "node_id is " << node_id;
    ++counts[node_id];
  }

  // In theory, these could fail even if the random selection is implemented
  // correctly, but it's very unlikely.
  EXPECT_GE(counts[2], 1600); EXPECT_LE(counts[2], 2400);
  EXPECT_GE(counts[4], 1600); EXPECT_LE(counts[4], 2400);
  EXPECT_GE(counts[6], 1600); EXPECT_LE(counts[6], 2400);

  // Once we unmark that group of nodes, the next node up should be node 7,
  // which has an ordered dependency on said group.
  EXPECT_TRUE(forest.MarkNoLongerReadyToWrite(2));
  EXPECT_TRUE(forest.MarkNoLongerReadyToWrite(4));
  EXPECT_TRUE(forest.MarkNoLongerReadyToWrite(6));
  EXPECT_FALSE(forest.MarkNoLongerReadyToWrite(99));
  EXPECT_EQ(7u, forest.NextNodeToWrite());

  ASSERT_TRUE(forest.ValidateInvariantsForTests());
}

}  // namespace net
