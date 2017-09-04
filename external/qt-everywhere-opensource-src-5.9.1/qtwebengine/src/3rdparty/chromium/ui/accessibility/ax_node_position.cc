// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_position.h"

#include "base/strings/string16.h"
#include "ui/accessibility/ax_enums.h"

namespace ui {

AXTree* AXNodePosition::tree_ = nullptr;

AXNodePosition::AXNodePosition() {}

AXNodePosition::~AXNodePosition() {}

void AXNodePosition::AnchorChild(int child_index,
                                 int* tree_id,
                                 int32_t* child_id) const {
  DCHECK(tree_id);
  DCHECK(child_id);

  if (!GetAnchor() || child_index < 0 || child_index >= AnchorChildCount()) {
    *tree_id = AXPosition::INVALID_TREE_ID;
    *child_id = AXPosition::INVALID_ANCHOR_ID;
    return;
  }

  AXNode* child = GetAnchor()->ChildAtIndex(child_index);
  DCHECK(child);
  *tree_id = this->tree_id();
  *child_id = child->id();
}

int AXNodePosition::AnchorChildCount() const {
  return GetAnchor() ? GetAnchor()->child_count() : 0;
}

int AXNodePosition::AnchorIndexInParent() const {
  return GetAnchor() ? GetAnchor()->index_in_parent()
                     : AXPosition::INVALID_INDEX;
}

void AXNodePosition::AnchorParent(int* tree_id, int32_t* parent_id) const {
  DCHECK(tree_id);
  DCHECK(parent_id);

  if (!GetAnchor() || !GetAnchor()->parent()) {
    *tree_id = AXPosition::INVALID_TREE_ID;
    *parent_id = AXPosition::INVALID_ANCHOR_ID;
    return;
  }

  AXNode* parent = GetAnchor()->parent();
  *tree_id = this->tree_id();
  *parent_id = parent->id();
}

AXNode* AXNodePosition::GetNodeInTree(int tree_id, int32_t node_id) const {
  if (!tree_ || node_id == AXPosition::INVALID_ANCHOR_ID)
    return nullptr;
  return AXNodePosition::tree_->GetFromId(node_id);
}

int AXNodePosition::MaxTextOffset() const {
  if (IsTextPosition()) {
    DCHECK(GetAnchor());
    base::string16 name =
        GetAnchor()->data().GetString16Attribute(AX_ATTR_NAME);
    return static_cast<int>(name.length());
  } else if (IsTreePosition()) {
    return 0;
  }

  return AXPosition::INVALID_INDEX;
}

}  // namespace ui
