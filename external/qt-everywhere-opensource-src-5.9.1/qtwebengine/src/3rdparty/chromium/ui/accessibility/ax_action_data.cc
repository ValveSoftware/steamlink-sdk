// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_action_data.h"

#include <set>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

using base::IntToString;

namespace ui {

AXActionData::AXActionData()
    : action(AX_ACTION_NONE),
      target_node_id(-1),
      flags(0),
      anchor_node_id(-1),
      anchor_offset(-1),
      focus_node_id(-1),
      focus_offset(-1) {
}

AXActionData::AXActionData(const AXActionData& other) = default;

AXActionData::~AXActionData() {
}

// Note that this includes an initial space character if nonempty, but
// that works fine because this is normally printed by AXAction::ToString.
std::string AXActionData::ToString() const {
  std::string result = ui::ToString(action);

  if (target_node_id != -1)
    result += " target_node_id=" + IntToString(target_node_id);

  if (flags & (1 << ui::AX_ACTION_FLAGS_REQUEST_IMAGES))
    result += " flag_request_images";

  if (flags & (1 << ui::AX_ACTION_FLAGS_REQUEST_INLINE_TEXT_BOXES))
    result += " flag_request_inline_text_boxes";

  if (anchor_node_id != -1) {
    result += " anchor_node_id=" + IntToString(anchor_node_id);
    result += " anchor_offset=" + IntToString(anchor_offset);
  }
  if (focus_node_id != -1) {
    result += " focus_node_id=" + IntToString(focus_node_id);
    result += " focus_offset=" + IntToString(focus_offset);
  }

  return result;
}

}  // namespace ui
