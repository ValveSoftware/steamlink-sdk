// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_data.h"

#include <set>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

using base::DoubleToString;
using base::IntToString;

namespace ui {

AXTreeData::AXTreeData()
    : tree_id(-1),
      parent_tree_id(-1),
      focused_tree_id(-1),
      loaded(false),
      loading_progress(0.0),
      focus_id(-1),
      sel_anchor_object_id(-1),
      sel_anchor_offset(-1),
      sel_focus_object_id(-1),
      sel_focus_offset(-1) {
}

AXTreeData::AXTreeData(const AXTreeData& other) = default;

AXTreeData::~AXTreeData() {
}

// Note that this includes an initial space character if nonempty, but
// that works fine because this is normally printed by AXTree::ToString.
std::string AXTreeData::ToString() const {
  std::string result;

  if (tree_id != -1)
    result += " tree_id=" + IntToString(tree_id);
  if (parent_tree_id != -1)
    result += " parent_tree_id=" + IntToString(parent_tree_id);
  if (focused_tree_id != -1)
    result += " focused_tree_id=" + IntToString(focused_tree_id);

  if (!doctype.empty())
    result += " doctype=" + doctype;
  if (loaded)
    result += " loaded=true";
  if (loading_progress != 0.0)
    result += " loading_progress=" + DoubleToString(loading_progress);
  if (!mimetype.empty())
    result += " mimetype=" + mimetype;
  if (!url.empty())
    result += " url=" + url;
  if (!title.empty())
    result += " title=" + title;

  if (focus_id != -1)
    result += " focus_id=" + IntToString(focus_id);

  if (sel_anchor_object_id != -1) {
    result += " sel_anchor_object_id=" + IntToString(sel_anchor_object_id);
    result += " sel_anchor_offset=" + IntToString(sel_anchor_offset);
  }
  if (sel_focus_object_id != -1) {
    result += " sel_focus_object_id=" + IntToString(sel_focus_object_id);
    result += " sel_focus_offset=" + IntToString(sel_focus_offset);
  }

  return result;
}

bool operator==(const AXTreeData& lhs, const AXTreeData& rhs) {
  return (lhs.tree_id == rhs.tree_id &&
          lhs.parent_tree_id == rhs.parent_tree_id &&
          lhs.focused_tree_id == rhs.focused_tree_id &&
          lhs.doctype == rhs.doctype && lhs.loaded == rhs.loaded &&
          lhs.loading_progress == rhs.loading_progress &&
          lhs.mimetype == rhs.mimetype && lhs.title == rhs.title &&
          lhs.url == rhs.url && lhs.focus_id == rhs.focus_id &&
          lhs.sel_anchor_object_id == rhs.sel_anchor_object_id &&
          lhs.sel_anchor_offset == rhs.sel_anchor_offset &&
          lhs.sel_focus_object_id == rhs.sel_focus_object_id &&
          lhs.sel_focus_offset == rhs.sel_focus_offset);
}

bool operator!=(const AXTreeData& lhs, const AXTreeData& rhs) {
  return !(lhs == rhs);
}

}  // namespace ui
