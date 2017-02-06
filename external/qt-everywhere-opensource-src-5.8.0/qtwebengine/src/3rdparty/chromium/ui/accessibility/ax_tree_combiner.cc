// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_combiner.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {
namespace {

// Return true if |attr| is a node ID that would need to be mapped when
// renumbering the ids in a combined tree.
bool IsNodeIdIntAttribute(AXIntAttribute attr) {
  switch (attr) {
    case AX_ATTR_ACTIVEDESCENDANT_ID:
    case AX_ATTR_MEMBER_OF_ID:
    case AX_ATTR_NEXT_ON_LINE_ID:
    case AX_ATTR_PREVIOUS_ON_LINE_ID:
    case AX_ATTR_TABLE_HEADER_ID:
    case AX_ATTR_TABLE_COLUMN_HEADER_ID:
    case AX_ATTR_TABLE_ROW_HEADER_ID:
      return true;

    // Note: all of the attributes are included here explicitly,
    // rather than using "default:", so that it's a compiler error to
    // add a new attribute without explicitly considering whether it's
    // a node id attribute or not.
    case AX_INT_ATTRIBUTE_NONE:
    case AX_ATTR_SCROLL_X:
    case AX_ATTR_SCROLL_X_MIN:
    case AX_ATTR_SCROLL_X_MAX:
    case AX_ATTR_SCROLL_Y:
    case AX_ATTR_SCROLL_Y_MIN:
    case AX_ATTR_SCROLL_Y_MAX:
    case AX_ATTR_TEXT_SEL_START:
    case AX_ATTR_TEXT_SEL_END:
    case AX_ATTR_TABLE_ROW_COUNT:
    case AX_ATTR_TABLE_COLUMN_COUNT:
    case AX_ATTR_TABLE_ROW_INDEX:
    case AX_ATTR_TABLE_COLUMN_INDEX:
    case AX_ATTR_TABLE_CELL_COLUMN_INDEX:
    case AX_ATTR_TABLE_CELL_COLUMN_SPAN:
    case AX_ATTR_TABLE_CELL_ROW_INDEX:
    case AX_ATTR_TABLE_CELL_ROW_SPAN:
    case AX_ATTR_SORT_DIRECTION:
    case AX_ATTR_HIERARCHICAL_LEVEL:
    case AX_ATTR_NAME_FROM:
    case AX_ATTR_DESCRIPTION_FROM:
    case AX_ATTR_CHILD_TREE_ID:
    case AX_ATTR_SET_SIZE:
    case AX_ATTR_POS_IN_SET:
    case AX_ATTR_COLOR_VALUE:
    case AX_ATTR_ARIA_CURRENT_STATE:
    case AX_ATTR_BACKGROUND_COLOR:
    case AX_ATTR_COLOR:
    case AX_ATTR_INVALID_STATE:
    case AX_ATTR_TEXT_DIRECTION:
    case AX_ATTR_TEXT_STYLE:
      return false;
  }

  NOTREACHED();
  return false;
}

// Return true if |attr| contains a vector of node ids that would need
// to be mapped when renumbering the ids in a combined tree.
bool IsNodeIdIntListAttribute(AXIntListAttribute attr) {
  switch (attr) {
    case AX_ATTR_CELL_IDS:
    case AX_ATTR_CONTROLS_IDS:
    case AX_ATTR_DESCRIBEDBY_IDS:
    case AX_ATTR_FLOWTO_IDS:
    case AX_ATTR_INDIRECT_CHILD_IDS:
    case AX_ATTR_LABELLEDBY_IDS:
    case AX_ATTR_UNIQUE_CELL_IDS:
      return true;

    // Note: all of the attributes are included here explicitly,
    // rather than using "default:", so that it's a compiler error to
    // add a new attribute without explicitly considering whether it's
    // a node id attribute or not.
    case AX_INT_LIST_ATTRIBUTE_NONE:
    case AX_ATTR_LINE_BREAKS:
    case AX_ATTR_MARKER_TYPES:
    case AX_ATTR_MARKER_STARTS:
    case AX_ATTR_MARKER_ENDS:
    case AX_ATTR_CHARACTER_OFFSETS:
    case AX_ATTR_WORD_STARTS:
    case AX_ATTR_WORD_ENDS:
      return false;
  }

  NOTREACHED();
  return false;
}

}  // namespace

AXTreeCombiner::AXTreeCombiner() {
}

AXTreeCombiner::~AXTreeCombiner() {
}

void AXTreeCombiner::AddTree(const AXTreeUpdate& tree, bool is_root) {
  trees_.push_back(tree);
  if (is_root) {
    DCHECK_EQ(root_tree_id_, -1);
    root_tree_id_ = tree.tree_data.tree_id;
  }
}

bool AXTreeCombiner::Combine() {
  // First create a map from tree ID to tree update.
  for (const auto& tree : trees_) {
    int32_t tree_id = tree.tree_data.tree_id;
    if (tree_id_map_.find(tree_id) != tree_id_map_.end())
      return false;
    tree_id_map_[tree.tree_data.tree_id] = &tree;
  }

  // Make sure the root tree ID is in the map, otherwise fail.
  if (tree_id_map_.find(root_tree_id_) == tree_id_map_.end())
    return false;

  // Process the nodes recursively, starting with the root tree.
  const AXTreeUpdate* root = tree_id_map_.find(root_tree_id_)->second;
  ProcessTree(root);

  // Set the root id.
  combined_.root_id = combined_.nodes[0].id;

  // Finally, handle the tree ID, taking into account which subtree might
  // have focus and mapping IDs from the tree data appropriately.
  combined_.has_tree_data = true;
  combined_.tree_data = root->tree_data;
  int32_t focused_tree_id = root->tree_data.focused_tree_id;
  const AXTreeUpdate* focused_tree = root;
  if (tree_id_map_.find(focused_tree_id) != tree_id_map_.end())
    focused_tree = tree_id_map_[focused_tree_id];
  combined_.tree_data.focus_id =
      MapId(focused_tree_id, focused_tree->tree_data.focus_id);
  combined_.tree_data.sel_anchor_object_id =
      MapId(focused_tree_id, focused_tree->tree_data.sel_anchor_object_id);
  combined_.tree_data.sel_focus_object_id =
      MapId(focused_tree_id, focused_tree->tree_data.sel_focus_object_id);
  combined_.tree_data.sel_anchor_offset =
      focused_tree->tree_data.sel_anchor_offset;
  combined_.tree_data.sel_focus_offset =
      focused_tree->tree_data.sel_focus_offset;

  // Debug-mode check that the resulting combined tree is valid.
  AXTree tree;
  DCHECK(tree.Unserialize(combined_))
      << combined_.ToString() << "\n" << tree.error();

  return true;
}

int32_t AXTreeCombiner::MapId(int32_t tree_id, int32_t node_id) {
  auto tree_id_node_id = std::make_pair(tree_id, node_id);
  if (tree_id_node_id_map_[tree_id_node_id] == 0)
    tree_id_node_id_map_[tree_id_node_id] = next_id_++;
  return tree_id_node_id_map_[tree_id_node_id];
}

void AXTreeCombiner::ProcessTree(const AXTreeUpdate* tree) {
  // The root of each tree may contain a transform that needs to apply
  // to all of its descendants.
  gfx::Transform old_transform = transform_;
  if (!tree->nodes.empty() && tree->nodes[0].transform)
    transform_.ConcatTransform(*tree->nodes[0].transform);

  int32_t tree_id = tree->tree_data.tree_id;
  for (size_t i = 0; i < tree->nodes.size(); ++i) {
    AXNodeData node = tree->nodes[i];
    int32_t child_tree_id = node.GetIntAttribute(AX_ATTR_CHILD_TREE_ID);

    // Map the node's ID.
    node.id = MapId(tree_id, node.id);

    // Map the node's child IDs.
    for (size_t j = 0; j < node.child_ids.size(); ++j)
      node.child_ids[j] = MapId(tree_id, node.child_ids[j]);

    // Map other int attributes that refer to node IDs, and remove the
    // AX_ATTR_CHILD_TREE_ID attribute.
    for (size_t j = 0; j < node.int_attributes.size(); ++j) {
      auto& attr = node.int_attributes[j];
      if (IsNodeIdIntAttribute(attr.first))
        attr.second = MapId(tree_id, attr.second);
      if (attr.first == AX_ATTR_CHILD_TREE_ID) {
        attr.first = AX_INT_ATTRIBUTE_NONE;
        attr.second = 0;
      }
    }

    // Map other int list attributes that refer to node IDs.
    for (size_t j = 0; j < node.intlist_attributes.size(); ++j) {
      auto& attr = node.intlist_attributes[j];
      if (IsNodeIdIntListAttribute(attr.first)) {
        for (size_t k = 0; k < attr.second.size(); k++)
          attr.second[k] = MapId(tree_id, attr.second[k]);
      }
    }

    // Apply the transformation to the object's bounds to put it in
    // the coordinate space of the root frame.
    gfx::RectF boundsf(node.location);
    transform_.TransformRect(&boundsf);
    node.location = gfx::Rect(boundsf.x(), boundsf.y(),
                              boundsf.width(), boundsf.height());

    // See if this node has a child tree. As a sanity check make sure the
    // child tree lists this tree as its parent tree id.
    const AXTreeUpdate* child_tree = nullptr;
    if (tree_id_map_.find(child_tree_id) != tree_id_map_.end()) {
      child_tree = tree_id_map_.find(child_tree_id)->second;
      if (child_tree->tree_data.parent_tree_id != tree_id)
        child_tree = nullptr;
      if (child_tree && child_tree->nodes.empty())
        child_tree = nullptr;
      if (child_tree) {
        node.child_ids.push_back(MapId(child_tree_id,
                                       child_tree->nodes[0].id));
      }
    }

    // Put the rewritten AXNodeData into the output data structure.
    combined_.nodes.push_back(node);

    // Recurse into the child tree now, if any.
    if (child_tree)
      ProcessTree(child_tree);
  }

  // Reset the transform.
  transform_ = old_transform;
}

}  // namespace ui
