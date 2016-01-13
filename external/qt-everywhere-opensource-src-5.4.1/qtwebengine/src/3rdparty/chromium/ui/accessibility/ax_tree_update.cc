// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_update.h"

#include "base/containers/hash_tables.h"
#include "base/strings/string_number_conversions.h"

namespace ui {

AXTreeUpdate::AXTreeUpdate() : node_id_to_clear(0) {
}

AXTreeUpdate::~AXTreeUpdate() {
}

std::string AXTreeUpdate::ToString() const {
  std::string result;
  if (node_id_to_clear != 0) {
    result += "AXTreeUpdate: clear node " +
        base::IntToString(node_id_to_clear) + "\n";
  }

  // The challenge here is that we want to indent the nodes being updated
  // so that parent/child relationships are clear, but we don't have access
  // to the rest of the tree for context, so we have to try to show the
  // relative indentation of child nodes in this update relative to their
  // parents.
  base::hash_map<int32, int> id_to_indentation;
  for (size_t i = 0; i < nodes.size(); ++i) {
    int indent = id_to_indentation[nodes[i].id];
    result += std::string(2 * indent, ' ');
    result += nodes[i].ToString() + "\n";
    for (size_t j = 0; j < nodes[i].child_ids.size(); ++j)
      id_to_indentation[nodes[i].child_ids[j]] = indent + 1;
  }

  return result;
}

}  // namespace ui
