// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_SERIALIZER_H_
#define UI_ACCESSIBILITY_AX_TREE_SERIALIZER_H_

#include <set>

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

struct ClientTreeNode;

// AXTreeSerializer is a helper class that serializes incremental
// updates to an AXTreeSource as a AXTreeUpdate struct.
// These structs can be unserialized by a client object such as an
// AXTree. An AXTreeSerializer keeps track of the tree of node ids that its
// client is aware of so that it will never generate an AXTreeUpdate that
// results in an invalid tree.
//
// Every node in the source tree must have an id that's a unique positive
// integer, the same node must not appear twice.
//
// Usage:
//
// You must call SerializeChanges() every time a node in the tree changes,
// and send the generated AXTreeUpdate to the client.
//
// If a node is added, call SerializeChanges on its parent.
// If a node is removed, call SerializeChanges on its parent.
// If a whole new subtree is added, just call SerializeChanges on its root.
// If the root of the tree changes, call SerializeChanges on the new root.
//
// AXTreeSerializer will avoid re-serializing nodes that do not change.
// For example, if node 1 has children 2, 3, 4, 5 and then child 2 is
// removed and a new child 6 is added, the AXTreeSerializer will only
// update nodes 1 and 6 (and any children of node 6 recursively). It will
// assume that nodes 3, 4, and 5 are not modified unless you explicitly
// call SerializeChanges() on them.
//
// As long as the source tree has unique ids for every node and no loops,
// and as long as every update is applied to the client tree, AXTreeSerializer
// will continue to work. If the source tree makes a change but fails to
// call SerializeChanges properly, the trees may get out of sync - but
// because AXTreeSerializer always keeps track of what updates it's sent,
// it will never send an invalid update and the client tree will not break,
// it just may not contain all of the changes.
template<typename AXSourceNode>
class AXTreeSerializer {
 public:
  explicit AXTreeSerializer(AXTreeSource<AXSourceNode>* tree);
  ~AXTreeSerializer();

  // Throw out the internal state that keeps track of the nodes the client
  // knows about. This has the effect that the next update will send the
  // entire tree over because it assumes the client knows nothing.
  void Reset();

  // Serialize all changes to |node| and append them to |out_update|.
  void SerializeChanges(AXSourceNode node,
                        AXTreeUpdate* out_update);

  // Delete the client subtree for this node, ensuring that the subtree
  // is re-serialized.
  void DeleteClientSubtree(AXSourceNode node);

  // Only for unit testing. Normally this class relies on getting a call
  // to SerializeChanges() every time the source tree changes. For unit
  // testing, it's convenient to create a static AXTree for the initial
  // state and then call ChangeTreeSourceForTesting and then SerializeChanges
  // to simulate the changes you'd get if a tree changed from the initial
  // state to the second tree's state.
  void ChangeTreeSourceForTesting(AXTreeSource<AXSourceNode>* new_tree);

 private:
  // Return the least common ancestor of a node in the source tree
  // and a node in the client tree, or NULL if there is no such node.
  // The least common ancestor is the closest ancestor to |node| (which
  // may be |node| itself) that's in both the source tree and client tree,
  // and for which both the source and client tree agree on their ancestor
  // chain up to the root.
  //
  // Example 1:
  //
  //    Client Tree    Source tree |
  //        1              1       |
  //       / \            / \      |
  //      2   3          2   4     |
  //
  // LCA(source node 2, client node 2) is node 2.
  // LCA(source node 3, client node 4) is node 1.
  //
  // Example 2:
  //
  //    Client Tree    Source tree |
  //        1              1       |
  //       / \            / \      |
  //      2   3          2   3     |
  //     / \            /   /      |
  //    4   7          8   4       |
  //   / \                / \      |
  //  5   6              5   6     |
  //
  // LCA(source node 8, client node 7) is node 2.
  // LCA(source node 5, client node 5) is node 1.
  // It's not node 5, because the two trees disagree on the parent of
  // node 4, so the LCA is the first ancestor both trees agree on.
  AXSourceNode LeastCommonAncestor(AXSourceNode node,
                                   ClientTreeNode* client_node);

  // Return the least common ancestor of |node| that's in the client tree.
  // This just walks up the ancestors of |node| until it finds a node that's
  // also in the client tree, and then calls LeastCommonAncestor on the
  // source node and client node.
  AXSourceNode LeastCommonAncestor(AXSourceNode node);

  // Walk the subtree rooted at |node| and return true if any nodes that
  // would be updated are being reparented. If so, update |out_lca| to point
  // to the least common ancestor of the previous LCA and the previous
  // parent of the node being reparented.
  bool AnyDescendantWasReparented(AXSourceNode node,
                                  AXSourceNode* out_lca);

  ClientTreeNode* ClientTreeNodeById(int32 id);

  // Delete the given client tree node and recursively delete all of its
  // descendants.
  void DeleteClientSubtree(ClientTreeNode* client_node);

  // Helper function, called recursively with each new node to serialize.
  void SerializeChangedNodes(AXSourceNode node,
                             AXTreeUpdate* out_update);

  // The tree source.
  AXTreeSource<AXSourceNode>* tree_;

  // Our representation of the client tree.
  ClientTreeNode* client_root_;

  // A map from IDs to nodes in the client tree.
  base::hash_map<int32, ClientTreeNode*> client_id_map_;
};

// In order to keep track of what nodes the client knows about, we keep a
// representation of the client tree - just IDs and parent/child
// relationships.
struct AX_EXPORT ClientTreeNode {
  ClientTreeNode();
  virtual ~ClientTreeNode();
  int32 id;
  ClientTreeNode* parent;
  std::vector<ClientTreeNode*> children;
};

template<typename AXSourceNode>
AXTreeSerializer<AXSourceNode>::AXTreeSerializer(
    AXTreeSource<AXSourceNode>* tree)
    : tree_(tree),
      client_root_(NULL) {
}

template<typename AXSourceNode>
AXTreeSerializer<AXSourceNode>::~AXTreeSerializer() {
  Reset();
}

template<typename AXSourceNode>
void AXTreeSerializer<AXSourceNode>::Reset() {
  if (!client_root_)
    return;

  DeleteClientSubtree(client_root_);
  client_id_map_.erase(client_root_->id);
  delete client_root_;
  client_root_ = NULL;
}

template<typename AXSourceNode>
void AXTreeSerializer<AXSourceNode>::ChangeTreeSourceForTesting(
    AXTreeSource<AXSourceNode>* new_tree) {
  tree_ = new_tree;
}

template<typename AXSourceNode>
AXSourceNode AXTreeSerializer<AXSourceNode>::LeastCommonAncestor(
    AXSourceNode node, ClientTreeNode* client_node) {
  if (!tree_->IsValid(node) || client_node == NULL)
    return tree_->GetNull();

  std::vector<AXSourceNode> ancestors;
  while (tree_->IsValid(node)) {
    ancestors.push_back(node);
    node = tree_->GetParent(node);
  }

  std::vector<ClientTreeNode*> client_ancestors;
  while (client_node) {
    client_ancestors.push_back(client_node);
    client_node = client_node->parent;
  }

  // Start at the root. Keep going until the source ancestor chain and
  // client ancestor chain disagree. The last node before they disagree
  // is the LCA.
  AXSourceNode lca = tree_->GetNull();
  int source_index = static_cast<int>(ancestors.size() - 1);
  int client_index = static_cast<int>(client_ancestors.size() - 1);
  while (source_index >= 0 && client_index >= 0) {
    if (tree_->GetId(ancestors[source_index]) !=
            client_ancestors[client_index]->id) {
      return lca;
    }
    lca = ancestors[source_index];
    source_index--;
    client_index--;
  }
  return lca;
}

template<typename AXSourceNode>
AXSourceNode AXTreeSerializer<AXSourceNode>::LeastCommonAncestor(
    AXSourceNode node) {
  // Walk up the tree until the source node's id also exists in the
  // client tree, then call LeastCommonAncestor on those two nodes.
  ClientTreeNode* client_node = ClientTreeNodeById(tree_->GetId(node));
  while (tree_->IsValid(node) && !client_node) {
    node = tree_->GetParent(node);
    if (tree_->IsValid(node))
      client_node = ClientTreeNodeById(tree_->GetId(node));
  }
  return LeastCommonAncestor(node, client_node);
}

template<typename AXSourceNode>
bool AXTreeSerializer<AXSourceNode>::AnyDescendantWasReparented(
    AXSourceNode node, AXSourceNode* out_lca) {
  bool result = false;
  int id = tree_->GetId(node);
  std::vector<AXSourceNode> children;
  tree_->GetChildren(node, &children);
  for (size_t i = 0; i < children.size(); ++i) {
    AXSourceNode& child = children[i];
    int child_id = tree_->GetId(child);
    ClientTreeNode* client_child = ClientTreeNodeById(child_id);
    if (client_child) {
      if (!client_child->parent) {
        // If the client child has no parent, it must have been the
        // previous root node, so there is no LCA and we can exit early.
        *out_lca = tree_->GetNull();
        return true;
      } else if (client_child->parent->id != id) {
        // If the client child's parent is not this node, update the LCA
        // and return true (reparenting was found).
        *out_lca = LeastCommonAncestor(*out_lca, client_child);
        result = true;
      } else {
        // This child is already in the client tree, we won't
        // recursively serialize it so we don't need to check this
        // subtree recursively for reparenting.
        continue;
      }
    }

    // This is a new child or reparented child, check it recursively.
    if (AnyDescendantWasReparented(child, out_lca))
      result = true;
  }
  return result;
}

template<typename AXSourceNode>
ClientTreeNode* AXTreeSerializer<AXSourceNode>::ClientTreeNodeById(int32 id) {
  base::hash_map<int32, ClientTreeNode*>::iterator iter =
      client_id_map_.find(id);
  if (iter != client_id_map_.end())
    return iter->second;
  else
    return NULL;
}

template<typename AXSourceNode>
void AXTreeSerializer<AXSourceNode>::SerializeChanges(
    AXSourceNode node,
    AXTreeUpdate* out_update) {
  // If the node isn't in the client tree, we need to serialize starting
  // with the LCA.
  AXSourceNode lca = LeastCommonAncestor(node);

  if (client_root_) {
    bool need_delete = false;
    if (tree_->IsValid(lca)) {
      // Check for any reparenting within this subtree - if there is
      // any, we need to delete and reserialize the whole subtree
      // that contains the old and new parents of the reparented node.
      if (AnyDescendantWasReparented(lca, &lca))
        need_delete = true;
    }

    if (!tree_->IsValid(lca)) {
      // If there's no LCA, just tell the client to destroy the whole
      // tree and then we'll serialize everything from the new root.
      out_update->node_id_to_clear = client_root_->id;
      Reset();
    } else if (need_delete) {
      // Otherwise, if we need to reserialize a subtree, first we need
      // to delete those nodes in our client tree so that
      // SerializeChangedNodes() will be sure to send them again.
      out_update->node_id_to_clear = tree_->GetId(lca);
      ClientTreeNode* client_lca = ClientTreeNodeById(tree_->GetId(lca));
      CHECK(client_lca);
      for (size_t i = 0; i < client_lca->children.size(); ++i) {
        client_id_map_.erase(client_lca->children[i]->id);
        DeleteClientSubtree(client_lca->children[i]);
        delete client_lca->children[i];
      }
      client_lca->children.clear();
    }
  }

  // Serialize from the LCA, or from the root if there isn't one.
  if (!tree_->IsValid(lca))
    lca = tree_->GetRoot();
  SerializeChangedNodes(lca, out_update);
}

template<typename AXSourceNode>
void AXTreeSerializer<AXSourceNode>::DeleteClientSubtree(AXSourceNode node) {
  ClientTreeNode* client_node = ClientTreeNodeById(tree_->GetId(node));
  if (client_node)
    DeleteClientSubtree(client_node);
}

template<typename AXSourceNode>
void AXTreeSerializer<AXSourceNode>::DeleteClientSubtree(
    ClientTreeNode* client_node) {
  for (size_t i = 0; i < client_node->children.size(); ++i) {
    client_id_map_.erase(client_node->children[i]->id);
    DeleteClientSubtree(client_node->children[i]);
    delete client_node->children[i];
  }
  client_node->children.clear();
}

template<typename AXSourceNode>
void AXTreeSerializer<AXSourceNode>::SerializeChangedNodes(
    AXSourceNode node,
    AXTreeUpdate* out_update) {
  // This method has three responsibilities:
  // 1. Serialize |node| into an AXNodeData, and append it to
  //    the AXTreeUpdate to be sent to the client.
  // 2. Determine if |node| has any new children that the client doesn't
  //    know about yet, and call SerializeChangedNodes recursively on those.
  // 3. Update our internal data structure that keeps track of what nodes
  //    the client knows about.

  // First, find the ClientTreeNode for this id in our data structure where
  // we keep track of what accessibility objects the client already knows
  // about. If we don't find it, then this must be the new root of the
  // accessibility tree.
  int id = tree_->GetId(node);
  ClientTreeNode* client_node = ClientTreeNodeById(id);
  if (!client_node) {
    Reset();
    client_root_ = new ClientTreeNode();
    client_node = client_root_;
    client_node->id = id;
    client_node->parent = NULL;
    client_id_map_[client_node->id] = client_node;
  }

  // Iterate over the ids of the children of |node|.
  // Create a set of the child ids so we can quickly look
  // up which children are new and which ones were there before.
  base::hash_set<int32> new_child_ids;
  std::vector<AXSourceNode> children;
  tree_->GetChildren(node, &children);
  for (size_t i = 0; i < children.size(); ++i) {
    AXSourceNode& child = children[i];
    int new_child_id = tree_->GetId(child);
    new_child_ids.insert(new_child_id);

    // This is a sanity check - there shouldn't be any reparenting
    // because we've already handled it above.
    ClientTreeNode* client_child = client_id_map_[new_child_id];
    CHECK(!client_child || client_child->parent == client_node);
  }

  // Go through the old children and delete subtrees for child
  // ids that are no longer present, and create a map from
  // id to ClientTreeNode for the rest. It's important to delete
  // first in a separate pass so that nodes that are reparented
  // don't end up children of two different parents in the middle
  // of an update, which can lead to a double-free.
  base::hash_map<int32, ClientTreeNode*> client_child_id_map;
  std::vector<ClientTreeNode*> old_children;
  old_children.swap(client_node->children);
  for (size_t i = 0; i < old_children.size(); ++i) {
    ClientTreeNode* old_child = old_children[i];
    int old_child_id = old_child->id;
    if (new_child_ids.find(old_child_id) == new_child_ids.end()) {
      client_id_map_.erase(old_child_id);
      DeleteClientSubtree(old_child);
      delete old_child;
    } else {
      client_child_id_map[old_child_id] = old_child;
    }
  }

  // Serialize this node. This fills in all of the fields in
  // AXNodeData except child_ids, which we handle below.
  out_update->nodes.push_back(AXNodeData());
  AXNodeData* serialized_node = &out_update->nodes.back();
  tree_->SerializeNode(node, serialized_node);
  // TODO(dmazzoni/dtseng): Make the serializer not depend on roles to identify
  // the root.
  if (serialized_node->id == client_root_->id &&
      (serialized_node->role != AX_ROLE_ROOT_WEB_AREA &&
       serialized_node->role != AX_ROLE_DESKTOP)) {
    serialized_node->role = AX_ROLE_ROOT_WEB_AREA;
  }
  serialized_node->child_ids.clear();

  // Iterate over the children, make note of the ones that are new
  // and need to be serialized, and update the ClientTreeNode
  // data structure to reflect the new tree.
  std::vector<AXSourceNode> children_to_serialize;
  client_node->children.reserve(children.size());
  for (size_t i = 0; i < children.size(); ++i) {
    AXSourceNode& child = children[i];
    int child_id = tree_->GetId(child);

    // No need to do anything more with children that aren't new;
    // the client will reuse its existing object.
    if (new_child_ids.find(child_id) == new_child_ids.end())
      continue;

    new_child_ids.erase(child_id);
    serialized_node->child_ids.push_back(child_id);
    if (client_child_id_map.find(child_id) != client_child_id_map.end()) {
      ClientTreeNode* reused_child = client_child_id_map[child_id];
      client_node->children.push_back(reused_child);
    } else {
      ClientTreeNode* new_child = new ClientTreeNode();
      new_child->id = child_id;
      new_child->parent = client_node;
      client_node->children.push_back(new_child);
      client_id_map_[child_id] = new_child;
      children_to_serialize.push_back(child);
    }
  }

  // Serialize all of the new children, recursively.
  for (size_t i = 0; i < children_to_serialize.size(); ++i)
    SerializeChangedNodes(children_to_serialize[i], out_update);
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_SERIALIZER_H_
