// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_POSITION_H_
#define UI_ACCESSIBILITY_AX_NODE_POSITION_H_

#include <stdint.h>

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

class AX_EXPORT AXNodePosition : public AXPosition<AXNodePosition, AXNode> {
 public:
  AXNodePosition();
  ~AXNodePosition() override;

  static void SetTreeForTesting(AXTree* tree) { tree_ = tree; }

 protected:
  void AnchorChild(int child_index,
                   int* tree_id,
                   int32_t* child_id) const override;
  int AnchorChildCount() const override;
  int AnchorIndexInParent() const override;
  void AnchorParent(int* tree_id, int32_t* parent_id) const override;
  AXNode* GetNodeInTree(int tree_id, int32_t node_id) const override;
  int MaxTextOffset() const override;

 private:
  static AXTree* tree_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_POSITION_H_
