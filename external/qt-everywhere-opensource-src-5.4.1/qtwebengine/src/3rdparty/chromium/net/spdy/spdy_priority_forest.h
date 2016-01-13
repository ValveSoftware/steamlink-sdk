// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_PRIORITY_FOREST_H_
#define NET_SPDY_SPDY_PRIORITY_FOREST_H_

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"

namespace net {

// This data structure implements the SPDY prioriziation data structures
// defined in this document: http://go/spdy4-prioritization
//
// Nodes can be added and removed, and dependencies between them defined.  Each
// node can have at most one parent and at most one child (forming a list), but
// there can be multiple lists, with each list root having its own priority.
// Individual nodes can also be marked as ready to read/write, and then the
// whole structure can be queried to pick the next node to read/write out of
// those ready.
//
// The NodeId and Priority types must be POD that support comparison (most
// likely, they will be numbers).
template <typename NodeId, typename Priority>
class SpdyPriorityForest {
 public:
  SpdyPriorityForest();
  ~SpdyPriorityForest();

  // Return the number of nodes currently in the forest.
  int num_nodes() const;

  // Return true if the forest contains a node with the given ID.
  bool NodeExists(NodeId node_id) const;

  // Add a new root node to the forest, with the given priority.  Returns true
  // on success, or false if the node_id already exists within the forest.
  bool AddRootNode(NodeId node_id, Priority priority);

  // Add a new node to the forest, with the given parent.  Returns true on
  // success.  Returns false and has no effect if the new node already exists,
  // or if the parent doesn't exist, or if the parent already has a child.
  bool AddNonRootNode(NodeId node_id, NodeId parent_id, bool unordered);

  // Remove an existing node from the forest.  Returns true on success, or
  // false if the node doesn't exist.
  bool RemoveNode(NodeId node_id);

  // Get the priority of the given node.  If the node doesn't exist, or is not
  // a root node (and thus has no priority), returns Priority().
  Priority GetPriority(NodeId node_id) const;

  // Get the parent of the given node.  If the node doesn't exist, or is a root
  // node (and thus has no parent), returns NodeId().
  NodeId GetParent(NodeId node_id) const;

  // Determine if the given node is unordered with respect to its parent.  If
  // the node doesn't exist, or is a root node (and thus has no parent),
  // returns false.
  bool IsNodeUnordered(NodeId node_id) const;

  // Get the child of the given node.  If the node doesn't exist, or has no
  // child, returns NodeId().
  NodeId GetChild(NodeId node_id) const;

  // Set the priority of the given node.  If the node was not already a root
  // node, this makes it a root node.  Returns true on success, or false if the
  // node doesn't exist.
  bool SetPriority(NodeId node_id, Priority priority);

  // Set the parent of the given node.  If the node was a root node, this makes
  // it no longer a root.  Returns true on success.  Returns false and has no
  // effect if (1) the node and/or the parent doesn't exist, (2) the new parent
  // already has a different child than the node, or (3) if the new parent is a
  // descendant of the node (so this would have created a cycle).
  bool SetParent(NodeId node_id, NodeId parent_id, bool unordered);

  // Check if a node is marked as ready to read.  Returns false if the node
  // doesn't exist.
  bool IsMarkedReadyToRead(NodeId node_id) const;
  // Mark the node as ready or not ready to read.  Returns true on success, or
  // false if the node doesn't exist.
  bool MarkReadyToRead(NodeId node_id);
  bool MarkNoLongerReadyToRead(NodeId node_id);
  // Return the ID of the next node that we should read, or return NodeId() if
  // no node in the forest is ready to read.
  NodeId NextNodeToRead();

  // Check if a node is marked as ready to write.  Returns false if the node
  // doesn't exist.
  bool IsMarkedReadyToWrite(NodeId node_id) const;
  // Mark the node as ready or not ready to write.  Returns true on success, or
  // false if the node doesn't exist.
  bool MarkReadyToWrite(NodeId node_id);
  bool MarkNoLongerReadyToWrite(NodeId node_id);
  // Return the ID of the next node that we should write, or return NodeId() if
  // no node in the forest is ready to write.
  NodeId NextNodeToWrite();

  // Return true if all internal invariants hold (useful for unit tests).
  // Unless there are bugs, this should always return true.
  bool ValidateInvariantsForTests() const;

 private:
  enum NodeType { ROOT_NODE, NONROOT_ORDERED, NONROOT_UNORDERED };
  struct Node {
    Node() : type(ROOT_NODE), flags(0), child() {
      depends_on.priority = Priority();
    }
    NodeType type;
    unsigned int flags;  // bitfield of flags
    union {
      Priority priority;  // used for root nodes
      NodeId parent_id;  // used for non-root nodes
    } depends_on;
    NodeId child;  // node ID of child (or NodeId() for no child)
  };

  typedef base::hash_map<NodeId, Node> NodeMap;

  // Constants for the Node.flags bitset:
  // kReadToRead: set for nodes that are ready for reading
  static const unsigned int kReadyToRead = (1 << 0);
  // kReadToWrite: set for nodes that are ready for writing
  static const unsigned int kReadyToWrite = (1 << 1);

  // Common code for IsMarkedReadyToRead and IsMarkedReadyToWrite.
  bool IsMarked(NodeId node_id, unsigned int flag) const;
  // Common code for MarkReadyToRead and MarkReadyToWrite.
  bool Mark(NodeId node_id, unsigned int flag);
  // Common code for MarkNoLongerReadyToRead and MarkNoLongerReadyToWrite.
  bool Unmark(NodeId node_id, unsigned int flag);
  // Common code for NextNodeToRead and NextNodeToWrite;
  NodeId FirstMarkedNode(unsigned int flag);
  // Get the given node, or return NULL if it doesn't exist.
  const Node* FindNode(NodeId node_id) const;

  NodeMap all_nodes_;  // maps from node IDs to Node objects

  DISALLOW_COPY_AND_ASSIGN(SpdyPriorityForest);
};

template <typename NodeId, typename Priority>
SpdyPriorityForest<NodeId, Priority>::SpdyPriorityForest() {}

template <typename NodeId, typename Priority>
SpdyPriorityForest<NodeId, Priority>::~SpdyPriorityForest() {}

template <typename NodeId, typename Priority>
int SpdyPriorityForest<NodeId, Priority>::num_nodes() const {
  return all_nodes_.size();
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::NodeExists(NodeId node_id) const {
  return all_nodes_.count(node_id) != 0;
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::AddRootNode(
    NodeId node_id, Priority priority) {
  if (NodeExists(node_id)) {
    return false;
  }
  Node* new_node = &all_nodes_[node_id];
  new_node->type = ROOT_NODE;
  new_node->depends_on.priority = priority;
  return true;
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::AddNonRootNode(
    NodeId node_id, NodeId parent_id, bool unordered) {
  if (NodeExists(node_id) || !NodeExists(parent_id)) {
    return false;
  }

  Node* parent = &all_nodes_[parent_id];
  if (parent->child != NodeId()) {
    return false;
  }

  Node* new_node = &all_nodes_[node_id];
  new_node->type = (unordered ? NONROOT_UNORDERED : NONROOT_ORDERED);
  new_node->depends_on.parent_id = parent_id;
  parent->child = node_id;
  return true;
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::RemoveNode(NodeId node_id) {
  if (!NodeExists(node_id)) {
    return false;
  }
  const Node& node = all_nodes_[node_id];

  // If the node to be removed is not a root node, we need to change its
  // parent's child ID.
  if (node.type != ROOT_NODE) {
    DCHECK(NodeExists(node.depends_on.parent_id));
    Node* parent = &all_nodes_[node.depends_on.parent_id];
    DCHECK_EQ(node_id, parent->child);
    parent->child = node.child;
  }

  // If the node has a child, we need to change the child's priority or parent.
  if (node.child != NodeId()) {
    DCHECK(NodeExists(node.child));
    Node* child = &all_nodes_[node.child];
    DCHECK_NE(ROOT_NODE, child->type);
    DCHECK_EQ(node_id, child->depends_on.parent_id);
    // Make the child's new depends_on be the node's depends_on (whether that
    // be a priority or a parent node ID).
    child->depends_on = node.depends_on;
    // If the removed node was a root, its child is now a root.  Otherwise, the
    // child will be be unordered if and only if it was already unordered and
    // the removed not is also not ordered.
    if (node.type == ROOT_NODE) {
      child->type = ROOT_NODE;
    } else if (node.type == NONROOT_ORDERED) {
      child->type = NONROOT_ORDERED;
    }
  }

  // Delete the node.
  all_nodes_.erase(node_id);
  return true;
}

template <typename NodeId, typename Priority>
Priority SpdyPriorityForest<NodeId, Priority>::GetPriority(
    NodeId node_id) const {
  const Node* node = FindNode(node_id);
  if (node != NULL && node->type == ROOT_NODE) {
    return node->depends_on.priority;
  } else {
    return Priority();
  }
}

template <typename NodeId, typename Priority>
NodeId SpdyPriorityForest<NodeId, Priority>::GetParent(NodeId node_id) const {
  const Node* node = FindNode(node_id);
  if (node != NULL && node->type != ROOT_NODE) {
    return node->depends_on.parent_id;
  } else {
    return NodeId();
  }
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::IsNodeUnordered(
    NodeId node_id) const {
  const Node* node = FindNode(node_id);
  return node != NULL && node->type == NONROOT_UNORDERED;
}

template <typename NodeId, typename Priority>
NodeId SpdyPriorityForest<NodeId, Priority>::GetChild(NodeId node_id) const {
  const Node* node = FindNode(node_id);
  if (node != NULL) {
    return node->child;
  } else {
    return NodeId();
  }
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::SetPriority(
    NodeId node_id, Priority priority) {
  if (!NodeExists(node_id)) {
    return false;
  }

  Node* node = &all_nodes_[node_id];
  // If this is not already a root node, we need to make it be a root node.
  if (node->type != ROOT_NODE) {
    DCHECK(NodeExists(node->depends_on.parent_id));
    Node* parent = &all_nodes_[node->depends_on.parent_id];
    parent->child = NodeId();
    node->type = ROOT_NODE;
  }

  node->depends_on.priority = priority;
  return true;
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::SetParent(
    NodeId node_id, NodeId parent_id, bool unordered) {
  if (!NodeExists(node_id) || !NodeExists(parent_id)) {
    return false;
  }

  Node* node = &all_nodes_[node_id];
  Node* new_parent = &all_nodes_[parent_id];
  // If the new parent is already the node's parent, all we have to do is
  // update the node type and we're done.
  if (new_parent->child == node_id) {
    node->type = (unordered ? NONROOT_UNORDERED : NONROOT_ORDERED);
    return true;
  }
  // Otherwise, if the new parent already has a child, we fail.
  if (new_parent->child != NodeId()) {
    return false;
  }

  // Next, make sure we won't create a cycle.
  if (node_id == parent_id) return false;
  Node* last = node;
  NodeId last_id = node_id;
  while (last->child != NodeId()) {
    if (last->child == parent_id) return false;
    last_id = last->child;
    DCHECK(NodeExists(last_id));
    last = &all_nodes_[last_id];
  }

  // If the node is not a root, we need clear its old parent's child field
  // (unless the old parent is the same as the new parent).
  if (node->type != ROOT_NODE) {
    const NodeId old_parent_id = node->depends_on.parent_id;
    DCHECK(NodeExists(old_parent_id));
    DCHECK(old_parent_id != parent_id);
    Node* old_parent = &all_nodes_[old_parent_id];
    DCHECK_EQ(node_id, old_parent->child);
    old_parent->child = NodeId();
  }

  // Make the change.
  node->type = (unordered ? NONROOT_UNORDERED : NONROOT_ORDERED);
  node->depends_on.parent_id = parent_id;
  new_parent->child = node_id;
  return true;
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::IsMarkedReadyToRead(
    NodeId node_id) const {
  return IsMarked(node_id, kReadyToRead);
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::MarkReadyToRead(NodeId node_id) {
  return Mark(node_id, kReadyToRead);
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::MarkNoLongerReadyToRead(
    NodeId node_id) {
  return Unmark(node_id, kReadyToRead);
}

template <typename NodeId, typename Priority>
NodeId SpdyPriorityForest<NodeId, Priority>::NextNodeToRead() {
  return FirstMarkedNode(kReadyToRead);
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::IsMarkedReadyToWrite(
    NodeId node_id) const {
  return IsMarked(node_id, kReadyToWrite);
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::MarkReadyToWrite(NodeId node_id) {
  return Mark(node_id, kReadyToWrite);
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::MarkNoLongerReadyToWrite(
    NodeId node_id) {
  return Unmark(node_id, kReadyToWrite);
}

template <typename NodeId, typename Priority>
NodeId SpdyPriorityForest<NodeId, Priority>::NextNodeToWrite() {
  return FirstMarkedNode(kReadyToWrite);
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::IsMarked(
    NodeId node_id, unsigned int flag) const {
  const Node* node = FindNode(node_id);
  return node != NULL && (node->flags & flag) != 0;
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::Mark(
    NodeId node_id, unsigned int flag) {
  if (!NodeExists(node_id)) {
    return false;
  }
  all_nodes_[node_id].flags |= flag;
  return true;
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::Unmark(
    NodeId node_id, unsigned int flag) {
  if (!NodeExists(node_id)) {
    return false;
  }
  all_nodes_[node_id].flags &= ~flag;
  return true;
}

template <typename NodeId, typename Priority>
NodeId SpdyPriorityForest<NodeId, Priority>::FirstMarkedNode(
    unsigned int flag) {
  // TODO(mdsteele): This is an *incredibly* stupid brute force solution.

  // Get all root nodes that have at least one marked child.
  uint64 total_weight = 0;
  std::map<uint64, NodeId> roots;  // maps cumulative weight to root node ID
  for (typename NodeMap::const_iterator iter = all_nodes_.begin();
       iter != all_nodes_.end(); ++iter) {
    const NodeId root_id = iter->first;
    const Node& root = iter->second;
    if (root.type == ROOT_NODE) {
      // See if there is at least one marked node in this root's chain.
      for (const Node* node = &root; ; node = &all_nodes_[node->child]) {
        if ((node->flags & flag) != 0) {
          total_weight += static_cast<uint64>(root.depends_on.priority);
          roots[total_weight] = root_id;
          break;
        }
        if (node->child == NodeId()) {
          break;
        }
        DCHECK(NodeExists(node->child));
      }
    }
  }

  // If there are no ready nodes, then return NodeId().
  if (total_weight == 0) {
    DCHECK(roots.empty());
    return NodeId();
  } else {
    DCHECK(!roots.empty());
  }

  // Randomly select a tree to use.
  typename std::map<uint64, NodeId>::const_iterator root_iter =
      roots.upper_bound(base::RandGenerator(total_weight));
  DCHECK(root_iter != roots.end());
  const NodeId root_id = root_iter->second;

  // Find the first node in the chain that is ready.
  NodeId node_id = root_id;
  while (true) {
    DCHECK(NodeExists(node_id));
    Node* node = &all_nodes_[node_id];
    if ((node->flags & flag) != 0) {
      // There might be more nodes that are ready and that are linked to this
      // one in an unordered chain.  Find all of them, then pick one randomly.
      std::vector<NodeId> group;
      group.push_back(node_id);
      for (Node* next = node; next->child != NodeId();) {
        DCHECK(NodeExists(next->child));
        Node *child = &all_nodes_[next->child];
        DCHECK_NE(ROOT_NODE, child->type);
        if (child->type != NONROOT_UNORDERED) {
          break;
        }
        if ((child->flags & flag) != 0) {
          group.push_back(next->child);
        }
        next = child;
      }
      return group[base::RandGenerator(group.size())];
    }
    node_id = node->child;
  }
}

template <typename NodeId, typename Priority>
const typename SpdyPriorityForest<NodeId, Priority>::Node*
SpdyPriorityForest<NodeId, Priority>::FindNode(NodeId node_id) const {
  typename NodeMap::const_iterator iter = all_nodes_.find(node_id);
  if (iter == all_nodes_.end()) {
    return NULL;
  }
  return &iter->second;
}

template <typename NodeId, typename Priority>
bool SpdyPriorityForest<NodeId, Priority>::ValidateInvariantsForTests() const {
  for (typename NodeMap::const_iterator iter = all_nodes_.begin();
       iter != all_nodes_.end(); ++iter) {
    const NodeId node_id = iter->first;
    const Node& node = iter->second;
    if (node.type != ROOT_NODE &&
        (!NodeExists(node.depends_on.parent_id) ||
         GetChild(node.depends_on.parent_id) != node_id)) {
      return false;
    }
    if (node.child != NodeId()) {
      if (!NodeExists(node.child) || node_id != GetParent(node.child)) {
        return false;
      }
    }

    NodeId child_id = node.child;
    int count = 0;
    while (child_id != NodeId()) {
      if (count > num_nodes() || node_id == child_id) {
        return false;
      }
      child_id = GetChild(child_id);
      ++count;
    }
  }
  return true;
}

}  // namespace net

#endif  // NET_SPDY_SPDY_PRIORITY_FOREST_H_
