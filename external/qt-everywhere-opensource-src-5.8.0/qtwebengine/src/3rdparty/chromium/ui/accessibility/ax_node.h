// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_H_
#define UI_ACCESSIBILITY_AX_NODE_H_

#include <stdint.h>

#include <vector>

#include "ui/accessibility/ax_node_data.h"

namespace gfx {
class Rect;
}

namespace ui {

// One node in an AXTree.
class AX_EXPORT AXNode {
 public:
  // The constructor requires a parent, id, and index in parent, but
  // the data is not required. After initialization, only index_in_parent
  // is allowed to change, the others are guaranteed to never change.
  AXNode(AXNode* parent, int32_t id, int32_t index_in_parent);
  virtual ~AXNode();

  // Accessors.
  int32_t id() const { return data_.id; }
  AXNode* parent() const { return parent_; }
  int child_count() const { return static_cast<int>(children_.size()); }
  const AXNodeData& data() const { return data_; }
  const std::vector<AXNode*>& children() const { return children_; }
  int index_in_parent() const { return index_in_parent_; }

  // Get the child at the given index.
  AXNode* ChildAtIndex(int index) const { return children_[index]; }

  // Set the node's accessibility data. This may be done during initial
  // initialization or later when the node data changes.
  void SetData(const AXNodeData& src);

  // Update this node's location. This is separate from SetData just because
  // changing only the location is common and should be more efficient than
  // re-copying all of the data.
  void SetLocation(const gfx::Rect& new_location);

  // Set the index in parent, for example if siblings were inserted or deleted.
  void SetIndexInParent(int index_in_parent);

  // Swap the internal children vector with |children|. This instance
  // now owns all of the passed children.
  void SwapChildren(std::vector<AXNode*>& children);

  // This is called when the AXTree no longer includes this node in the
  // tree. Reference counting is used on some platforms because the
  // operating system may hold onto a reference to an AXNode
  // object even after we're through with it, so this may decrement the
  // reference count and clear out the object's data.
  void Destroy();

  // Return true if this object is equal to or a descendant of |ancestor|.
  bool IsDescendantOf(AXNode* ancestor);

 private:
  int index_in_parent_;
  AXNode* parent_;
  std::vector<AXNode*> children_;
  AXNodeData data_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_H_
