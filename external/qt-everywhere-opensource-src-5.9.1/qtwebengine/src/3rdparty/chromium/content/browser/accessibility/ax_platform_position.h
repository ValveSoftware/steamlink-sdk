// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_AX_PLATFORM_POSITION_H_
#define CONTENT_BROWSER_ACCESSIBILITY_AX_PLATFORM_POSITION_H_

#include <stdint.h>

#include "content/browser/accessibility/ax_tree_id_registry.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "ui/accessibility/ax_position.h"

namespace content {

using AXTreeID = content::AXTreeIDRegistry::AXTreeID;

class AXPlatformPosition
    : public ui::AXPosition<AXPlatformPosition, BrowserAccessibility> {
 public:
  AXPlatformPosition();
  ~AXPlatformPosition() override;

 protected:
  void AnchorChild(int child_index,
                   AXTreeID* tree_id,
                   int32_t* child_id) const override;
  int AnchorChildCount() const override;
  int AnchorIndexInParent() const override;
  void AnchorParent(AXTreeID* tree_id, int32_t* parent_id) const override;
  BrowserAccessibility* GetNodeInTree(AXTreeID tree_id,
                                      int32_t node_id) const override;
  int MaxTextOffset() const override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_AX_PLATFORM_POSITION_H_
