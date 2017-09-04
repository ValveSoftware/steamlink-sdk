// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_H_
#define UI_ACCESSIBILITY_AX_TREE_H_

#include <stdint.h>

#include <set>

#include "base/containers/hash_tables.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

class AXNode;
class AXTree;
struct AXTreeUpdateState;

// Used when you want to be notified when changes happen to the tree.
//
// Some of the notifications are called in the middle of an update operation.
// Be careful, as the tree may be in an inconsistent state at this time;
// don't walk the parents and children at this time:
//   OnNodeWillBeDeleted
//   OnSubtreeWillBeDeleted
//   OnNodeWillBeReparented
//   OnSubtreeWillBeReparented
//   OnNodeCreated
//   OnNodeReparented
//   OnNodeChanged
//
// In addition, one additional notification is fired at the end of an
// atomic update, and it provides a vector of nodes that were added or
// changed, for final postprocessing:
//   OnAtomicUpdateFinished
//
class AX_EXPORT AXTreeDelegate {
 public:
  AXTreeDelegate();
  virtual ~AXTreeDelegate();

  // Called before a node's data gets updated.
  virtual void OnNodeDataWillChange(AXTree* tree,
                                    const AXNodeData& old_node_data,
                                    const AXNodeData& new_node_data) = 0;

  // Called when tree data changes.
  virtual void OnTreeDataChanged(AXTree* tree) = 0;

  // Called just before a node is deleted. Its id and data will be valid,
  // but its links to parents and children are invalid. This is called
  // in the middle of an update, the tree may be in an invalid state!
  virtual void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) = 0;

  // Same as OnNodeWillBeDeleted, but only called once for an entire subtree.
  // This is called in the middle of an update, the tree may be in an
  // invalid state!
  virtual void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) = 0;

  // Called just before a node is deleted for reparenting. See
  // |OnNodeWillBeDeleted| for additional information.
  virtual void OnNodeWillBeReparented(AXTree* tree, AXNode* node) = 0;

  // Called just before a subtree is deleted for reparenting. See
  // |OnSubtreeWillBeDeleted| for additional information.
  virtual void OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) = 0;

  // Called immediately after a new node is created. The tree may be in
  // the middle of an update, don't walk the parents and children now.
  virtual void OnNodeCreated(AXTree* tree, AXNode* node) = 0;

  // Called immediately after a node is reparented. The tree may be in the
  // middle of an update, don't walk the parents and children now.
  virtual void OnNodeReparented(AXTree* tree, AXNode* node) = 0;

  // Called when a node changes its data or children. The tree may be in
  // the middle of an update, don't walk the parents and children now.
  virtual void OnNodeChanged(AXTree* tree, AXNode* node) = 0;

  enum ChangeType {
    NODE_CREATED,
    SUBTREE_CREATED,
    NODE_CHANGED,
    NODE_REPARENTED,
    SUBTREE_REPARENTED
  };

  struct Change {
    Change(AXNode* node, ChangeType type) {
      this->node = node;
      this->type = type;
    }
    AXNode* node;
    ChangeType type;
  };

  // Called at the end of the update operation. Every node that was added
  // or changed will be included in |changes|, along with an enum indicating
  // the type of change - either (1) a node was created, (2) a node was created
  // and it's the root of a new subtree, or (3) a node was changed. Finally,
  // a bool indicates if the root of the tree was changed or not.
  virtual void OnAtomicUpdateFinished(AXTree* tree,
                                      bool root_changed,
                                      const std::vector<Change>& changes) = 0;
};

// AXTree is a live, managed tree of AXNode objects that can receive
// updates from another AXTreeSource via AXTreeUpdates, and it can be
// used as a source for sending updates to another client tree.
// It's designed to be subclassed to implement support for native
// accessibility APIs on a specific platform.
class AX_EXPORT AXTree {
 public:
  AXTree();
  explicit AXTree(const AXTreeUpdate& initial_state);
  virtual ~AXTree();

  virtual void SetDelegate(AXTreeDelegate* delegate);

  AXNode* root() const { return root_; }

  const AXTreeData& data() const { return data_; }

  // Returns the AXNode with the given |id| if it is part of this AXTree.
  AXNode* GetFromId(int32_t id) const;

  // Returns true on success. If it returns false, it's a fatal error
  // and this tree should be destroyed, and the source of the tree update
  // should not be trusted any longer.
  virtual bool Unserialize(const AXTreeUpdate& update);

  virtual void UpdateData(const AXTreeData& data);

  // Return a multi-line indented string representation, for logging.
  std::string ToString() const;

  // A string describing the error from an unsuccessful Unserialize,
  // for testing and debugging.
  const std::string& error() const { return error_; }

  int size() { return static_cast<int>(id_map_.size()); }

 private:
  AXNode* CreateNode(AXNode* parent,
                     int32_t id,
                     int32_t index_in_parent,
                     AXTreeUpdateState* update_state);

  // This is called from within Unserialize(), it returns true on success.
  bool UpdateNode(const AXNodeData& src,
                  bool is_new_root,
                  AXTreeUpdateState* update_state);

  void OnRootChanged();

  // Notify the delegate that the subtree rooted at |node| will be destroyed,
  // then call DestroyNodeAndSubtree on it.
  void DestroySubtree(AXNode* node, AXTreeUpdateState* update_state);

  // Call Destroy() on |node|, and delete it from the id map, and then
  // call recursively on all nodes in its subtree.
  void DestroyNodeAndSubtree(AXNode* node, AXTreeUpdateState* update_state);

  // Iterate over the children of |node| and for each child, destroy the
  // child and its subtree if its id is not in |new_child_ids|. Returns
  // true on success, false on fatal error.
  bool DeleteOldChildren(AXNode* node,
                         const std::vector<int32_t>& new_child_ids,
                         AXTreeUpdateState* update_state);

  // Iterate over |new_child_ids| and populate |new_children| with
  // pointers to child nodes, reusing existing nodes already in the tree
  // if they exist, and creating otherwise. Reparenting is disallowed, so
  // if the id already exists as the child of another node, that's an
  // error. Returns true on success, false on fatal error.
  bool CreateNewChildVector(AXNode* node,
                            const std::vector<int32_t>& new_child_ids,
                            std::vector<AXNode*>* new_children,
                            AXTreeUpdateState* update_state);

  AXTreeDelegate* delegate_;
  AXNode* root_;
  base::hash_map<int32_t, AXNode*> id_map_;
  std::string error_;
  AXTreeData data_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_H_
