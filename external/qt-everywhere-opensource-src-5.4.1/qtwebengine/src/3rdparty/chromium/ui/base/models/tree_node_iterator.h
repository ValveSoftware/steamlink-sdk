// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_TREE_NODE_ITERATOR_H_
#define UI_BASE_MODELS_TREE_NODE_ITERATOR_H_

#include <stack>

#include "base/basictypes.h"
#include "base/logging.h"

namespace ui {

// Iterator that iterates over the descendants of a node. The iteration does
// not include the node itself, only the descendants. The following illustrates
// typical usage:
// while (iterator.has_next()) {
//   Node* node = iterator.Next();
//   // do something with node.
// }
template <class NodeType>
class TreeNodeIterator {
 public:
  // This contructor accepts an optional filter function |prune| which could be
  // used to prune complete branches of the tree. The filter function will be
  // evaluated on each tree node and if it evaluates to true the node and all
  // its descendants will be skipped by the iterator.
  TreeNodeIterator(NodeType* node, bool (*prune)(NodeType*))
      : prune_(prune) {
    int index = 0;

    // Move forward through the children list until the first non prunable node.
    // This is to satisfy the iterator invariant that the current index in the
    // Position at the top of the _positions list must point to a node the
    // iterator will be returning.
    for (; index < node->child_count(); ++index)
      if (!prune || !prune(node->GetChild(index)))
        break;

    if (index < node->child_count())
      positions_.push(Position<NodeType>(node, index));
  }

  explicit TreeNodeIterator(NodeType* node) : prune_(NULL) {
    if (!node->empty())
      positions_.push(Position<NodeType>(node, 0));
  }

  // Returns true if there are more descendants.
  bool has_next() const { return !positions_.empty(); }

  // Returns the next descendant.
  NodeType* Next() {
    if (!has_next()) {
      NOTREACHED();
      return NULL;
    }

    // There must always be a valid node in the current Position index.
    NodeType* result = positions_.top().node->GetChild(positions_.top().index);

    // Make sure we don't attempt to visit result again.
    positions_.top().index++;

    // Iterate over result's children.
    positions_.push(Position<NodeType>(result, 0));

    // Advance to next valid node by skipping over the pruned nodes and the
    // empty Positions. At the end of this loop two cases are possible:
    // - the current index of the top() Position points to a valid node
    // - the _position list is empty, the iterator has_next() will return false.
    while (!positions_.empty()) {
      if (positions_.top().index >= positions_.top().node->child_count())
        positions_.pop(); // This Position is all processed, move to the next.
      else if (prune_ &&
          prune_(positions_.top().node->GetChild(positions_.top().index)))
        positions_.top().index++;  // Prune the branch.
      else
        break;  // Now positioned at the next node to be returned.
    }

    return result;
  }

 private:
  template <class PositionNodeType>
  struct Position {
    Position(PositionNodeType* node, int index) : node(node), index(index) {}
    Position() : node(NULL), index(-1) {}

    PositionNodeType* node;
    int index;
  };

  std::stack<Position<NodeType> > positions_;
  bool (*prune_)(NodeType*);

  DISALLOW_COPY_AND_ASSIGN(TreeNodeIterator);
};

}  // namespace ui

#endif  // UI_BASE_MODELS_TREE_NODE_ITERATOR_H_
