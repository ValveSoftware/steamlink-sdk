// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_TREE_NODE_MODEL_H_
#define UI_BASE_MODELS_TREE_NODE_MODEL_H_

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "ui/base/models/tree_model.h"

namespace ui {

// TreeNodeModel and TreeNodes provide an implementation of TreeModel around
// TreeNodes.
//
// TreeNodes own their children, so that deleting a node deletes all
// descendants.
//
// TreeNodes do NOT maintain a pointer back to the model. As such, if you
// are using TreeNodes with a TreeNodeModel you will need to notify the observer
// yourself any time you make any change directly to the TreeNodes. For example,
// if you directly invoke set_title on a node it does not notify the observer,
// you will need to do it yourself. This includes the following methods: Add,
// Remove and set_title. TreeNodeModel provides cover methods that mutate the
// TreeNodes and notify the observer. If you are using TreeNodes with a
// TreeNodeModel use the cover methods to save yourself the headache.
//
// The following example creates a TreeNode with two children and then
// creates a TreeNodeModel from it:
//
// TreeNodeWithValue<int>* root = new TreeNodeWithValue<int>();
// root->Add(new TreeNodeWithValue<int>(ASCIIToUTF16("child 1"), 0));
// root->Add(new TreeNodeWithValue<int>(ASCIIToUTF16("child 2"), 1));
// TreeNodeModel<TreeNodeWithValue<int> > model(root);
//
// Two variants of TreeNode are provided here:
//
// . TreeNode itself is intended for subclassing. It has one type parameter
//   that corresponds to the type of the node. When subclassing use your class
//   name as the type parameter, eg:
//   class MyTreeNode : public TreeNode<MyTreeNode> .
// . TreeNodeWithValue is a trivial subclass of TreeNode that has one type
//   type parameter: a value type that is associated with the node.
//
// Which you use depends upon the situation. If you want to subclass and add
// methods, then use TreeNode. If you don't need any extra methods and just
// want to associate a value with each node, then use TreeNodeWithValue.
//
// Regardless of which TreeNode you use, if you are using the nodes with a
// TreeView take care to notify the observer when mutating the nodes.

// TreeNode -------------------------------------------------------------------

template <class NodeType>
class TreeNode : public TreeModelNode {
 public:
  TreeNode() : parent_(NULL) {}

  explicit TreeNode(const base::string16& title)
      : title_(title), parent_(NULL) {}

  ~TreeNode() override {
    STLDeleteElements(&children_);
  }

  // Adds |node| as a child of this node, at |index|.
  virtual void Add(NodeType* node, int index) {
    DCHECK(node);
    DCHECK_GE(index, 0);
    DCHECK_LE(index, child_count());
    // If |node| has a parent, remove it from its parent.
    NodeType* parent = node->parent_;
    if (parent)
      parent->Remove(node);
    node->parent_ = static_cast<NodeType*>(this);
    children_.insert(children_.begin() + index, node);
  }

  // Removes |node| from this node and returns it. It's up to the caller to
  // delete it.
  virtual NodeType* Remove(NodeType* node) {
    typename std::vector<NodeType*>::iterator i =
        std::find(children_.begin(), children_.end(), node);
    DCHECK(i != children_.end());
    node->parent_ = NULL;
    children_.erase(i);
    return node;
  }

  // Removes all the children from this node. This does NOT delete the nodes.
  void RemoveAll() {
    for (size_t i = 0; i < children_.size(); ++i)
      children_[i]->parent_ = NULL;
    children_.clear();
  }

  // Removes all existing children without deleting the nodes and adds all nodes
  // contained in |children| into this node as children.
  void SetChildren(const std::vector<NodeType*>& children) {
    RemoveAll();
    for (size_t i = 0; i < children.size(); ++i)
      Add(children[i], static_cast<int>(i));
  }

  // Returns the parent node, or NULL if this is the root node.
  const NodeType* parent() const { return parent_; }
  NodeType* parent() { return parent_; }

  // Returns true if this is the root node.
  bool is_root() const { return parent_ == NULL; }

  // Returns the number of children.
  int child_count() const { return static_cast<int>(children_.size()); }

  // Returns true if this node has no children.
  bool empty() const { return children_.empty(); }

  // Returns the number of all nodes in the subtree rooted at this node,
  // including this node.
  int GetTotalNodeCount() const {
    int count = 1;  // Start with one to include the node itself.
    for (size_t i = 0; i < children_.size(); ++i)
      count += children_[i]->GetTotalNodeCount();
    return count;
  }

  // Returns the node at |index|.
  const NodeType* GetChild(int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, child_count());
    return children_[index];
  }
  NodeType* GetChild(int index) {
    return const_cast<NodeType*>(
        static_cast<const NodeType&>(*this).GetChild(index));
  }

  // Returns the index of |node|, or -1 if |node| is not a child of this.
  int GetIndexOf(const NodeType* node) const {
    DCHECK(node);
    typename std::vector<NodeType*>::const_iterator i =
        std::find(children_.begin(), children_.end(), node);
    return i != children_.end() ? static_cast<int>(i - children_.begin()) : -1;
  }

  // Sets the title of the node.
  virtual void SetTitle(const base::string16& title) { title_ = title; }

  // TreeModelNode:
  const base::string16& GetTitle() const override { return title_; }

  // Returns true if this == ancestor, or one of this nodes parents is
  // ancestor.
  bool HasAncestor(const NodeType* ancestor) const {
    if (ancestor == this)
      return true;
    if (!ancestor)
      return false;
    return parent_ ? parent_->HasAncestor(ancestor) : false;
  }

 protected:
  std::vector<NodeType*>& children() { return children_; }

 private:
  // Title displayed in the tree.
  base::string16 title_;

  // This node's parent.
  NodeType* parent_;

  // This node's children.
  std::vector<NodeType*> children_;

  DISALLOW_COPY_AND_ASSIGN(TreeNode);
};

// TreeNodeWithValue ----------------------------------------------------------

template <class ValueType>
class TreeNodeWithValue : public TreeNode< TreeNodeWithValue<ValueType> > {
 public:
  TreeNodeWithValue() {}

  explicit TreeNodeWithValue(const ValueType& value)
      : ParentType(base::string16()), value(value) {}

  TreeNodeWithValue(const base::string16& title, const ValueType& value)
      : ParentType(title), value(value) {}

  ValueType value;

 private:
  typedef TreeNode< TreeNodeWithValue<ValueType> > ParentType;

  DISALLOW_COPY_AND_ASSIGN(TreeNodeWithValue);
};

// TreeNodeModel --------------------------------------------------------------

// TreeModel implementation intended to be used with TreeNodes.
template <class NodeType>
class TreeNodeModel : public TreeModel {
 public:
  // Creates a TreeNodeModel with the specified root node. The root is owned
  // by the TreeNodeModel.
  explicit TreeNodeModel(NodeType* root) : root_(root) {}
  virtual ~TreeNodeModel() override {}

  NodeType* AsNode(TreeModelNode* model_node) {
    return static_cast<NodeType*>(model_node);
  }

  void Add(NodeType* parent, NodeType* node, int index) {
    DCHECK(parent && node);
    parent->Add(node, index);
    NotifyObserverTreeNodesAdded(parent, index, 1);
  }

  NodeType* Remove(NodeType* parent, NodeType* node) {
    DCHECK(parent);
    int index = parent->GetIndexOf(node);
    NodeType* delete_node = parent->Remove(node);
    NotifyObserverTreeNodesRemoved(parent, index, 1);
    return delete_node;
  }

  void NotifyObserverTreeNodesAdded(NodeType* parent, int start, int count) {
    FOR_EACH_OBSERVER(TreeModelObserver,
                      observer_list_,
                      TreeNodesAdded(this, parent, start, count));
  }

  void NotifyObserverTreeNodesRemoved(NodeType* parent, int start, int count) {
    FOR_EACH_OBSERVER(TreeModelObserver,
                      observer_list_,
                      TreeNodesRemoved(this, parent, start, count));
  }

  void NotifyObserverTreeNodeChanged(TreeModelNode* node) {
    FOR_EACH_OBSERVER(TreeModelObserver,
                      observer_list_,
                      TreeNodeChanged(this, node));
  }

  // TreeModel:
  NodeType* GetRoot() override {
    return root_.get();
  }

  int GetChildCount(TreeModelNode* parent) override {
    DCHECK(parent);
    return AsNode(parent)->child_count();
  }

  NodeType* GetChild(TreeModelNode* parent, int index) override {
    DCHECK(parent);
    return AsNode(parent)->GetChild(index);
  }

  int GetIndexOf(TreeModelNode* parent, TreeModelNode* child) override {
    DCHECK(parent);
    return AsNode(parent)->GetIndexOf(AsNode(child));
  }

  TreeModelNode* GetParent(TreeModelNode* node) override {
    DCHECK(node);
    return AsNode(node)->parent();
  }

  void AddObserver(TreeModelObserver* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(TreeModelObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void SetTitle(TreeModelNode* node,
                const base::string16& title) override {
    DCHECK(node);
    AsNode(node)->SetTitle(title);
    NotifyObserverTreeNodeChanged(node);
  }

 private:
  // The observers.
  base::ObserverList<TreeModelObserver> observer_list_;

  // The root.
  std::unique_ptr<NodeType> root_;

  DISALLOW_COPY_AND_ASSIGN(TreeNodeModel);
};

}  // namespace ui

#endif  // UI_BASE_MODELS_TREE_NODE_MODEL_H_
