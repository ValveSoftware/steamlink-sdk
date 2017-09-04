// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ACTION_DATA_H_
#define UI_ACCESSIBILITY_AX_ACTION_DATA_H_

#include "base/strings/string16.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

// A compact representation of an accessibility action and the arguments
// associated with that action.
struct AX_EXPORT AXActionData {
  AXActionData();
  AXActionData(const AXActionData& other);
  virtual ~AXActionData();

  // Return a string representation of this data, for debugging.
  virtual std::string ToString() const;

  // This is a simple serializable struct. All member variables should be
  // public and copyable.

  // See the AXAction enums in ax_enums.idl for explanations of which
  // parameters apply.

  // The action to take.
  AXAction action;

  // The ID of the node that this action should be performed on.
  int target_node_id;

  // Use enums from AXActionFlags
  int flags;

  // For an action that creates a selection, the selection anchor and focus
  // (see ax_tree_data.h for definitions).
  int anchor_node_id;
  int anchor_offset;

  int focus_node_id;
  int focus_offset;

  // The target rect for the action.
  gfx::Rect target_rect;

  // The target point for the action.
  gfx::Point target_point;

  // The new value for a node, for the SET_VALUE action.
  base::string16 value;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ACTION_DATA_H_
