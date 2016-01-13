// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_serializable_tree.h"

#include "ui/accessibility/ax_node.h"

namespace ui {

// This class is an implementation of the AXTreeSource interface with
// AXNode as the node type, that just delegates to an AXTree. The purpose
// of this is so that AXTreeSerializer only needs to work with the
// AXTreeSource abstraction and doesn't need to actually know about
// AXTree directly. Another AXTreeSource is used to abstract the Blink
// accessibility tree.
class AX_EXPORT AXTreeSourceAdapter : public AXTreeSource<const AXNode*> {
 public:
  AXTreeSourceAdapter(AXTree* tree) : tree_(tree) {}
  virtual ~AXTreeSourceAdapter() {}

  // AXTreeSource implementation.
  virtual AXNode* GetRoot() const OVERRIDE {
    return tree_->GetRoot();
  }

  virtual AXNode* GetFromId(int32 id) const OVERRIDE {
    return tree_->GetFromId(id);
  }

  virtual int32 GetId(const AXNode* node) const OVERRIDE {
    return node->id();
  }

  virtual void GetChildren(
      const AXNode* node,
      std::vector<const AXNode*>* out_children) const OVERRIDE {
    for (int i = 0; i < node->child_count(); ++i)
      out_children->push_back(node->ChildAtIndex(i));
  }

  virtual AXNode* GetParent(const AXNode* node) const OVERRIDE {
    return node->parent();
  }

  virtual bool IsValid(const AXNode* node) const OVERRIDE {
    return node != NULL;
  }

  virtual bool IsEqual(const AXNode* node1,
                       const AXNode* node2) const OVERRIDE {
    return node1 == node2;
  }

  virtual const AXNode* GetNull() const OVERRIDE {
    return NULL;
  }

  virtual void SerializeNode(
      const AXNode* node, AXNodeData* out_data) const OVERRIDE {
    *out_data = node->data();
  }

 private:
  AXTree* tree_;
};

AXSerializableTree::AXSerializableTree()
    : AXTree() {}

AXSerializableTree::AXSerializableTree(const AXTreeUpdate& initial_state)
    : AXTree(initial_state) {
}

AXSerializableTree::~AXSerializableTree() {
}

AXTreeSource<const AXNode*>* AXSerializableTree::CreateTreeSource() {
  return new AXTreeSourceAdapter(this);
}

}  // namespace ui
