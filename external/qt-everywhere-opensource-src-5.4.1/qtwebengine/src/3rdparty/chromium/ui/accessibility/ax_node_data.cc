// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_data.h"

#include <set>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

using base::DoubleToString;
using base::IntToString;

namespace ui {

namespace {

std::string IntVectorToString(const std::vector<int>& items) {
  std::string str;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0)
      str += ",";
    str += IntToString(items[i]);
  }
  return str;
}

}  // Anonymous namespace

AXNodeData::AXNodeData()
    : id(-1),
      role(AX_ROLE_UNKNOWN),
      state(-1) {
}

AXNodeData::~AXNodeData() {
}

void AXNodeData::AddStringAttribute(
    AXStringAttribute attribute, const std::string& value) {
  string_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddIntAttribute(
    AXIntAttribute attribute, int value) {
  int_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddFloatAttribute(
    AXFloatAttribute attribute, float value) {
  float_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddBoolAttribute(
    AXBoolAttribute attribute, bool value) {
  bool_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddIntListAttribute(
    AXIntListAttribute attribute, const std::vector<int32>& value) {
  intlist_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::SetName(std::string name) {
  string_attributes.push_back(std::make_pair(AX_ATTR_NAME, name));
}

void AXNodeData::SetValue(std::string value) {
  string_attributes.push_back(std::make_pair(AX_ATTR_VALUE, value));
}

std::string AXNodeData::ToString() const {
  std::string result;

  result += "id=" + IntToString(id);
  result += " " + ui::ToString(role);

  if (state & (1 << ui::AX_STATE_BUSY))
    result += " BUSY";
  if (state & (1 << ui::AX_STATE_CHECKED))
    result += " CHECKED";
  if (state & (1 << ui::AX_STATE_COLLAPSED))
    result += " COLLAPSED";
  if (state & (1 << ui::AX_STATE_EXPANDED))
    result += " EXPANDED";
  if (state & (1 << ui::AX_STATE_FOCUSABLE))
    result += " FOCUSABLE";
  if (state & (1 << ui::AX_STATE_FOCUSED))
    result += " FOCUSED";
  if (state & (1 << ui::AX_STATE_HASPOPUP))
    result += " HASPOPUP";
  if (state & (1 << ui::AX_STATE_HOVERED))
    result += " HOVERED";
  if (state & (1 << ui::AX_STATE_INDETERMINATE))
    result += " INDETERMINATE";
  if (state & (1 << ui::AX_STATE_INVISIBLE))
    result += " INVISIBLE";
  if (state & (1 << ui::AX_STATE_LINKED))
    result += " LINKED";
  if (state & (1 << ui::AX_STATE_MULTISELECTABLE))
    result += " MULTISELECTABLE";
  if (state & (1 << ui::AX_STATE_OFFSCREEN))
    result += " OFFSCREEN";
  if (state & (1 << ui::AX_STATE_PRESSED))
    result += " PRESSED";
  if (state & (1 << ui::AX_STATE_PROTECTED))
    result += " PROTECTED";
  if (state & (1 << ui::AX_STATE_READ_ONLY))
    result += " READONLY";
  if (state & (1 << ui::AX_STATE_REQUIRED))
    result += " REQUIRED";
  if (state & (1 << ui::AX_STATE_SELECTABLE))
    result += " SELECTABLE";
  if (state & (1 << ui::AX_STATE_SELECTED))
    result += " SELECTED";
  if (state & (1 << ui::AX_STATE_VERTICAL))
    result += " VERTICAL";
  if (state & (1 << ui::AX_STATE_VISITED))
    result += " VISITED";

  result += " (" + IntToString(location.x()) + ", " +
                   IntToString(location.y()) + ")-(" +
                   IntToString(location.width()) + ", " +
                   IntToString(location.height()) + ")";

  for (size_t i = 0; i < int_attributes.size(); ++i) {
    std::string value = IntToString(int_attributes[i].second);
    switch (int_attributes[i].first) {
      case AX_ATTR_SCROLL_X:
        result += " scroll_x=" + value;
        break;
      case AX_ATTR_SCROLL_X_MIN:
        result += " scroll_x_min=" + value;
        break;
      case AX_ATTR_SCROLL_X_MAX:
        result += " scroll_x_max=" + value;
        break;
      case AX_ATTR_SCROLL_Y:
        result += " scroll_y=" + value;
        break;
      case AX_ATTR_SCROLL_Y_MIN:
        result += " scroll_y_min=" + value;
        break;
      case AX_ATTR_SCROLL_Y_MAX:
        result += " scroll_y_max=" + value;
        break;
      case AX_ATTR_HIERARCHICAL_LEVEL:
        result += " level=" + value;
        break;
      case AX_ATTR_TEXT_SEL_START:
        result += " sel_start=" + value;
        break;
      case AX_ATTR_TEXT_SEL_END:
        result += " sel_end=" + value;
        break;
      case AX_ATTR_TABLE_ROW_COUNT:
        result += " rows=" + value;
        break;
      case AX_ATTR_TABLE_COLUMN_COUNT:
        result += " cols=" + value;
        break;
      case AX_ATTR_TABLE_CELL_COLUMN_INDEX:
        result += " col=" + value;
        break;
      case AX_ATTR_TABLE_CELL_ROW_INDEX:
        result += " row=" + value;
        break;
      case AX_ATTR_TABLE_CELL_COLUMN_SPAN:
        result += " colspan=" + value;
        break;
      case AX_ATTR_TABLE_CELL_ROW_SPAN:
        result += " rowspan=" + value;
        break;
      case AX_ATTR_TABLE_COLUMN_HEADER_ID:
        result += " column_header_id=" + value;
        break;
      case AX_ATTR_TABLE_COLUMN_INDEX:
        result += " column_index=" + value;
        break;
      case AX_ATTR_TABLE_HEADER_ID:
        result += " header_id=" + value;
        break;
      case AX_ATTR_TABLE_ROW_HEADER_ID:
        result += " row_header_id=" + value;
        break;
      case AX_ATTR_TABLE_ROW_INDEX:
        result += " row_index=" + value;
        break;
      case AX_ATTR_TITLE_UI_ELEMENT:
        result += " title_elem=" + value;
        break;
      case AX_ATTR_ACTIVEDESCENDANT_ID:
        result += " activedescendant=" + value;
        break;
      case AX_ATTR_COLOR_VALUE_RED:
        result += " color_value_red=" + value;
        break;
      case AX_ATTR_COLOR_VALUE_GREEN:
        result += " color_value_green=" + value;
        break;
      case AX_ATTR_COLOR_VALUE_BLUE:
        result += " color_value_blue=" + value;
        break;
      case AX_ATTR_TEXT_DIRECTION:
        switch (int_attributes[i].second) {
          case AX_TEXT_DIRECTION_LR:
          default:
            result += " text_direction=lr";
            break;
          case AX_TEXT_DIRECTION_RL:
            result += " text_direction=rl";
            break;
          case AX_TEXT_DIRECTION_TB:
            result += " text_direction=tb";
            break;
          case AX_TEXT_DIRECTION_BT:
            result += " text_direction=bt";
            break;
        }
        break;
      case AX_INT_ATTRIBUTE_NONE:
        break;
    }
  }

  for (size_t i = 0; i < string_attributes.size(); ++i) {
    std::string value = string_attributes[i].second;
    switch (string_attributes[i].first) {
      case AX_ATTR_DOC_URL:
        result += " doc_url=" + value;
        break;
      case AX_ATTR_DOC_TITLE:
        result += " doc_title=" + value;
        break;
      case AX_ATTR_DOC_MIMETYPE:
        result += " doc_mimetype=" + value;
        break;
      case AX_ATTR_DOC_DOCTYPE:
        result += " doc_doctype=" + value;
        break;
      case AX_ATTR_ACCESS_KEY:
        result += " access_key=" + value;
        break;
      case AX_ATTR_ACTION:
        result += " action=" + value;
        break;
      case AX_ATTR_DESCRIPTION:
        result += " description=" + value;
        break;
      case AX_ATTR_DISPLAY:
        result += " display=" + value;
        break;
      case AX_ATTR_HELP:
        result += " help=" + value;
        break;
      case AX_ATTR_HTML_TAG:
        result += " html_tag=" + value;
        break;
      case AX_ATTR_LIVE_RELEVANT:
        result += " relevant=" + value;
        break;
      case AX_ATTR_LIVE_STATUS:
        result += " live=" + value;
        break;
      case AX_ATTR_CONTAINER_LIVE_RELEVANT:
        result += " container_relevant=" + value;
        break;
      case AX_ATTR_CONTAINER_LIVE_STATUS:
        result += " container_live=" + value;
        break;
      case AX_ATTR_ROLE:
        result += " role=" + value;
        break;
      case AX_ATTR_SHORTCUT:
        result += " shortcut=" + value;
        break;
      case AX_ATTR_URL:
        result += " url=" + value;
        break;
      case AX_ATTR_NAME:
        result += " name=" + value;
        break;
      case AX_ATTR_VALUE:
        result += " value=" + value;
        break;
      case AX_STRING_ATTRIBUTE_NONE:
        break;
    }
  }

  for (size_t i = 0; i < float_attributes.size(); ++i) {
    std::string value = DoubleToString(float_attributes[i].second);
    switch (float_attributes[i].first) {
      case AX_ATTR_DOC_LOADING_PROGRESS:
        result += " doc_progress=" + value;
        break;
      case AX_ATTR_VALUE_FOR_RANGE:
        result += " value_for_range=" + value;
        break;
      case AX_ATTR_MAX_VALUE_FOR_RANGE:
        result += " max_value=" + value;
        break;
      case AX_ATTR_MIN_VALUE_FOR_RANGE:
        result += " min_value=" + value;
        break;
      case AX_FLOAT_ATTRIBUTE_NONE:
        break;
    }
  }

  for (size_t i = 0; i < bool_attributes.size(); ++i) {
    std::string value = bool_attributes[i].second ? "true" : "false";
    switch (bool_attributes[i].first) {
      case AX_ATTR_DOC_LOADED:
        result += " doc_loaded=" + value;
        break;
      case AX_ATTR_BUTTON_MIXED:
        result += " mixed=" + value;
        break;
      case AX_ATTR_LIVE_ATOMIC:
        result += " atomic=" + value;
        break;
      case AX_ATTR_LIVE_BUSY:
        result += " busy=" + value;
        break;
      case AX_ATTR_CONTAINER_LIVE_ATOMIC:
        result += " container_atomic=" + value;
        break;
      case AX_ATTR_CONTAINER_LIVE_BUSY:
        result += " container_busy=" + value;
        break;
      case AX_ATTR_ARIA_READONLY:
        result += " aria_readonly=" + value;
        break;
      case AX_ATTR_CAN_SET_VALUE:
        result += " can_set_value=" + value;
        break;
      case AX_ATTR_UPDATE_LOCATION_ONLY:
        result += " update_location_only=" + value;
        break;
      case AX_ATTR_CANVAS_HAS_FALLBACK:
        result += " has_fallback=" + value;
        break;
      case AX_BOOL_ATTRIBUTE_NONE:
        break;
    }
  }

  for (size_t i = 0; i < intlist_attributes.size(); ++i) {
    const std::vector<int32>& values = intlist_attributes[i].second;
    switch (intlist_attributes[i].first) {
      case AX_ATTR_INDIRECT_CHILD_IDS:
        result += " indirect_child_ids=" + IntVectorToString(values);
        break;
      case AX_ATTR_CONTROLS_IDS:
        result += " controls_ids=" + IntVectorToString(values);
        break;
      case AX_ATTR_DESCRIBEDBY_IDS:
        result += " describedby_ids=" + IntVectorToString(values);
        break;
      case AX_ATTR_FLOWTO_IDS:
        result += " flowto_ids=" + IntVectorToString(values);
        break;
      case AX_ATTR_LABELLEDBY_IDS:
        result += " labelledby_ids=" + IntVectorToString(values);
        break;
      case AX_ATTR_OWNS_IDS:
        result += " owns_ids=" + IntVectorToString(values);
        break;
      case AX_ATTR_LINE_BREAKS:
        result += " line_breaks=" + IntVectorToString(values);
        break;
      case AX_ATTR_CELL_IDS:
        result += " cell_ids=" + IntVectorToString(values);
        break;
      case AX_ATTR_UNIQUE_CELL_IDS:
        result += " unique_cell_ids=" + IntVectorToString(values);
        break;
      case AX_ATTR_CHARACTER_OFFSETS:
        result += " character_offsets=" + IntVectorToString(values);
        break;
      case AX_ATTR_WORD_STARTS:
        result += " word_starts=" + IntVectorToString(values);
        break;
      case AX_ATTR_WORD_ENDS:
        result += " word_ends=" + IntVectorToString(values);
        break;
      case AX_INT_LIST_ATTRIBUTE_NONE:
        break;
    }
  }

  if (!child_ids.empty())
    result += " child_ids=" + IntVectorToString(child_ids);

  return result;
}

}  // namespace ui
