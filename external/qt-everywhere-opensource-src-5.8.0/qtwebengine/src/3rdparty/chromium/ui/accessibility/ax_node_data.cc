// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_data.h"

#include <stddef.h>

#include <algorithm>
#include <set>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/transform.h"

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

// Predicate that returns true if the first value of a pair is |first|.
template<typename FirstType, typename SecondType>
struct FirstIs {
  FirstIs(FirstType first)
      : first_(first) {}
  bool operator()(std::pair<FirstType, SecondType> const& p) {
    return p.first == first_;
  }
  FirstType first_;
};

// Helper function that finds a key in a vector of pairs by matching on the
// first value, and returns an iterator.
template<typename FirstType, typename SecondType>
typename std::vector<std::pair<FirstType, SecondType>>::const_iterator
    FindInVectorOfPairs(
        FirstType first,
        const std::vector<std::pair<FirstType, SecondType>>& vector) {
  return std::find_if(vector.begin(),
                      vector.end(),
                      FirstIs<FirstType, SecondType>(first));
}

}  // namespace

AXNodeData::AXNodeData()
    : id(-1),
      role(AX_ROLE_UNKNOWN),
      state(0xFFFFFFFF) {
}

AXNodeData::~AXNodeData() {
}

AXNodeData::AXNodeData(const AXNodeData& other) {
  id = other.id;
  role = other.role;
  state = other.state;
  string_attributes = other.string_attributes;
  int_attributes = other.int_attributes;
  float_attributes = other.float_attributes;
  bool_attributes = other.bool_attributes;
  intlist_attributes = other.intlist_attributes;
  html_attributes = other.html_attributes;
  child_ids = other.child_ids;
  location = other.location;
  if (other.transform)
    transform.reset(new gfx::Transform(*other.transform));
}

AXNodeData& AXNodeData::operator=(AXNodeData other) {
  id = other.id;
  role = other.role;
  state = other.state;
  string_attributes = other.string_attributes;
  int_attributes = other.int_attributes;
  float_attributes = other.float_attributes;
  bool_attributes = other.bool_attributes;
  intlist_attributes = other.intlist_attributes;
  html_attributes = other.html_attributes;
  child_ids = other.child_ids;
  location = other.location;
  if (other.transform)
    transform.reset(new gfx::Transform(*other.transform));
  return *this;
}

bool AXNodeData::HasBoolAttribute(AXBoolAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, bool_attributes);
  return iter != bool_attributes.end();
}

bool AXNodeData::GetBoolAttribute(AXBoolAttribute attribute) const {
  bool result;
  if (GetBoolAttribute(attribute, &result))
    return result;
  return false;
}

bool AXNodeData::GetBoolAttribute(
    AXBoolAttribute attribute, bool* value) const {
  auto iter = FindInVectorOfPairs(attribute, bool_attributes);
  if (iter != bool_attributes.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool AXNodeData::HasFloatAttribute(AXFloatAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, float_attributes);
  return iter != float_attributes.end();
}

float AXNodeData::GetFloatAttribute(AXFloatAttribute attribute) const {
  float result;
  if (GetFloatAttribute(attribute, &result))
    return result;
  return 0.0;
}

bool AXNodeData::GetFloatAttribute(
    AXFloatAttribute attribute, float* value) const {
  auto iter = FindInVectorOfPairs(attribute, float_attributes);
  if (iter != float_attributes.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool AXNodeData::HasIntAttribute(AXIntAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, int_attributes);
  return iter != int_attributes.end();
}

int AXNodeData::GetIntAttribute(AXIntAttribute attribute) const {
  int result;
  if (GetIntAttribute(attribute, &result))
    return result;
  return 0;
}

bool AXNodeData::GetIntAttribute(
    AXIntAttribute attribute, int* value) const {
  auto iter = FindInVectorOfPairs(attribute, int_attributes);
  if (iter != int_attributes.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool AXNodeData::HasStringAttribute(AXStringAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, string_attributes);
  return iter != string_attributes.end();
}

const std::string& AXNodeData::GetStringAttribute(
    AXStringAttribute attribute) const {
  CR_DEFINE_STATIC_LOCAL(std::string, empty_string, ());
  auto iter = FindInVectorOfPairs(attribute, string_attributes);
  return iter != string_attributes.end() ? iter->second : empty_string;
}

bool AXNodeData::GetStringAttribute(
    AXStringAttribute attribute, std::string* value) const {
  auto iter = FindInVectorOfPairs(attribute, string_attributes);
  if (iter != string_attributes.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

base::string16 AXNodeData::GetString16Attribute(
    AXStringAttribute attribute) const {
  std::string value_utf8;
  if (!GetStringAttribute(attribute, &value_utf8))
    return base::string16();
  return base::UTF8ToUTF16(value_utf8);
}

bool AXNodeData::GetString16Attribute(
    AXStringAttribute attribute,
    base::string16* value) const {
  std::string value_utf8;
  if (!GetStringAttribute(attribute, &value_utf8))
    return false;
  *value = base::UTF8ToUTF16(value_utf8);
  return true;
}

bool AXNodeData::HasIntListAttribute(AXIntListAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, intlist_attributes);
  return iter != intlist_attributes.end();
}

const std::vector<int32_t>& AXNodeData::GetIntListAttribute(
    AXIntListAttribute attribute) const {
  CR_DEFINE_STATIC_LOCAL(std::vector<int32_t>, empty_vector, ());
  auto iter = FindInVectorOfPairs(attribute, intlist_attributes);
  if (iter != intlist_attributes.end())
    return iter->second;
  return empty_vector;
}

bool AXNodeData::GetIntListAttribute(AXIntListAttribute attribute,
                                     std::vector<int32_t>* value) const {
  auto iter = FindInVectorOfPairs(attribute, intlist_attributes);
  if (iter != intlist_attributes.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool AXNodeData::GetHtmlAttribute(
    const char* html_attr, std::string* value) const {
  for (size_t i = 0; i < html_attributes.size(); ++i) {
    const std::string& attr = html_attributes[i].first;
    if (base::LowerCaseEqualsASCII(attr, html_attr)) {
      *value = html_attributes[i].second;
      return true;
    }
  }

  return false;
}

bool AXNodeData::GetHtmlAttribute(
    const char* html_attr, base::string16* value) const {
  std::string value_utf8;
  if (!GetHtmlAttribute(html_attr, &value_utf8))
    return false;
  *value = base::UTF8ToUTF16(value_utf8);
  return true;
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

void AXNodeData::AddIntListAttribute(AXIntListAttribute attribute,
                                     const std::vector<int32_t>& value) {
  intlist_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::SetName(const std::string& name) {
  string_attributes.push_back(std::make_pair(AX_ATTR_NAME, name));
}

void AXNodeData::SetValue(const std::string& value) {
  string_attributes.push_back(std::make_pair(AX_ATTR_VALUE, value));
}

std::string AXNodeData::ToString() const {
  std::string result;

  result += "id=" + IntToString(id);
  result += " " + ui::ToString(role);

  if (state & (1 << AX_STATE_BUSY))
    result += " BUSY";
  if (state & (1 << AX_STATE_CHECKED))
    result += " CHECKED";
  if (state & (1 << AX_STATE_COLLAPSED))
    result += " COLLAPSED";
  if (state & (1 << AX_STATE_EDITABLE))
    result += " EDITABLE";
  if (state & (1 << AX_STATE_EXPANDED))
    result += " EXPANDED";
  if (state & (1 << AX_STATE_FOCUSABLE))
    result += " FOCUSABLE";
  if (state & (1 << AX_STATE_HASPOPUP))
    result += " HASPOPUP";
  if (state & (1 << AX_STATE_HOVERED))
    result += " HOVERED";
  if (state & (1 << AX_STATE_INVISIBLE))
    result += " INVISIBLE";
  if (state & (1 << AX_STATE_LINKED))
    result += " LINKED";
  if (state & (1 << AX_STATE_MULTISELECTABLE))
    result += " MULTISELECTABLE";
  if (state & (1 << AX_STATE_OFFSCREEN))
    result += " OFFSCREEN";
  if (state & (1 << AX_STATE_PRESSED))
    result += " PRESSED";
  if (state & (1 << AX_STATE_PROTECTED))
    result += " PROTECTED";
  if (state & (1 << AX_STATE_READ_ONLY))
    result += " READONLY";
  if (state & (1 << AX_STATE_REQUIRED))
    result += " REQUIRED";
  if (state & (1 << AX_STATE_RICHLY_EDITABLE))
    result += " RICHLY_EDITABLE";
  if (state & (1 << AX_STATE_SELECTABLE))
    result += " SELECTABLE";
  if (state & (1 << AX_STATE_SELECTED))
    result += " SELECTED";
  if (state & (1 << AX_STATE_VERTICAL))
    result += " VERTICAL";
  if (state & (1 << AX_STATE_VISITED))
    result += " VISITED";

  result += " (" + IntToString(location.x()) + ", " +
                   IntToString(location.y()) + ")-(" +
                   IntToString(location.width()) + ", " +
                   IntToString(location.height()) + ")";

  if (transform && !transform->IsIdentity())
    result += " transform=" + transform->ToString();

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
      case AX_ATTR_SORT_DIRECTION:
        switch (int_attributes[i].second) {
          case AX_SORT_DIRECTION_UNSORTED:
            result += " sort_direction=none";
            break;
          case AX_SORT_DIRECTION_ASCENDING:
            result += " sort_direction=ascending";
            break;
          case AX_SORT_DIRECTION_DESCENDING:
            result += " sort_direction=descending";
            break;
          case AX_SORT_DIRECTION_OTHER:
            result += " sort_direction=other";
            break;
        }
        break;
      case AX_ATTR_NAME_FROM:
        result += " name_from=" + ui::ToString(
            static_cast<ui::AXNameFrom>(int_attributes[i].second));
        break;
      case AX_ATTR_DESCRIPTION_FROM:
        result += " description_from=" + ui::ToString(
            static_cast<ui::AXDescriptionFrom>(int_attributes[i].second));
        break;
      case AX_ATTR_ACTIVEDESCENDANT_ID:
        result += " activedescendant=" + value;
        break;
      case AX_ATTR_MEMBER_OF_ID:
        result += " member_of_id=" + value;
        break;
      case AX_ATTR_NEXT_ON_LINE_ID:
        result += " next_on_line_id=" + value;
        break;
      case AX_ATTR_PREVIOUS_ON_LINE_ID:
        result += " previous_on_line_id=" + value;
        break;
      case AX_ATTR_CHILD_TREE_ID:
        result += " child_tree_id=" + value;
        break;
      case AX_ATTR_COLOR_VALUE:
        result += base::StringPrintf(" color_value=&%X",
                                     int_attributes[i].second);
        break;
      case AX_ATTR_ARIA_CURRENT_STATE:
        switch (int_attributes[i].second) {
          case AX_ARIA_CURRENT_STATE_FALSE:
            result += " aria_current_state=false";
            break;
          case AX_ARIA_CURRENT_STATE_TRUE:
            result += " aria_current_state=true";
            break;
          case AX_ARIA_CURRENT_STATE_PAGE:
            result += " aria_current_state=page";
            break;
          case AX_ARIA_CURRENT_STATE_STEP:
            result += " aria_current_state=step";
            break;
          case AX_ARIA_CURRENT_STATE_LOCATION:
            result += " aria_current_state=location";
            break;
          case AX_ARIA_CURRENT_STATE_DATE:
            result += " aria_current_state=date";
            break;
          case AX_ARIA_CURRENT_STATE_TIME:
            result += " aria_current_state=time";
            break;
        }
        break;
      case AX_ATTR_BACKGROUND_COLOR:
        result += base::StringPrintf(" background_color=&%X",
                                     int_attributes[i].second);
        break;
      case AX_ATTR_COLOR:
        result += base::StringPrintf(" color=&%X", int_attributes[i].second);
        break;
      case AX_ATTR_TEXT_DIRECTION:
        switch (int_attributes[i].second) {
          case AX_TEXT_DIRECTION_LTR:
            result += " text_direction=ltr";
            break;
          case AX_TEXT_DIRECTION_RTL:
            result += " text_direction=rtl";
            break;
          case AX_TEXT_DIRECTION_TTB:
            result += " text_direction=ttb";
            break;
          case AX_TEXT_DIRECTION_BTT:
            result += " text_direction=btt";
            break;
        }
        break;
      case AX_ATTR_TEXT_STYLE: {
        auto text_style = static_cast<AXTextStyle>(int_attributes[i].second);
        if (text_style == AX_TEXT_STYLE_NONE)
          break;
        std::string text_style_value(" text_style=");
        if (text_style & AX_TEXT_STYLE_BOLD)
          text_style_value += "bold,";
        if (text_style & AX_TEXT_STYLE_ITALIC)
          text_style_value += "italic,";
        if (text_style & AX_TEXT_STYLE_UNDERLINE)
          text_style_value += "underline,";
        if (text_style & AX_TEXT_STYLE_LINE_THROUGH)
          text_style_value += "line-through,";
        result += text_style_value.substr(0, text_style_value.size() - 1);
        break;
      }
      case AX_ATTR_SET_SIZE:
        result += " setsize=" + value;
        break;
      case AX_ATTR_POS_IN_SET:
        result += " posinset=" + value;
        break;
      case AX_ATTR_INVALID_STATE:
        switch (int_attributes[i].second) {
          case AX_INVALID_STATE_FALSE:
            result += " invalid_state=false";
            break;
          case AX_INVALID_STATE_TRUE:
            result += " invalid_state=true";
            break;
          case AX_INVALID_STATE_SPELLING:
            result += " invalid_state=spelling";
            break;
          case AX_INVALID_STATE_GRAMMAR:
            result += " invalid_state=grammar";
            break;
          case AX_INVALID_STATE_OTHER:
            result += " invalid_state=other";
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
      case AX_ATTR_ACCESS_KEY:
        result += " access_key=" + value;
        break;
      case AX_ATTR_ACTION:
        result += " action=" + value;
        break;
      case AX_ATTR_ARIA_INVALID_VALUE:
        result += " aria_invalid_value=" + value;
        break;
      case AX_ATTR_AUTO_COMPLETE:
        result += " autocomplete=" + value;
        break;
      case AX_ATTR_DESCRIPTION:
        result += " description=" + value;
        break;
      case AX_ATTR_DISPLAY:
        result += " display=" + value;
        break;
      case AX_ATTR_FONT_FAMILY:
        result += " font-family=" + value;
        break;
      case AX_ATTR_HTML_TAG:
        result += " html_tag=" + value;
        break;
      case AX_ATTR_LANGUAGE:
        result += " language=" + value;
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
      case AX_ATTR_PLACEHOLDER:
        result += " placeholder=" + value;
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
      case AX_ATTR_VALUE_FOR_RANGE:
        result += " value_for_range=" + value;
        break;
      case AX_ATTR_MAX_VALUE_FOR_RANGE:
        result += " max_value=" + value;
        break;
      case AX_ATTR_MIN_VALUE_FOR_RANGE:
        result += " min_value=" + value;
        break;
      case AX_ATTR_FONT_SIZE:
        result += " font_size=" + value;
        break;
      case AX_FLOAT_ATTRIBUTE_NONE:
        break;
    }
  }

  for (size_t i = 0; i < bool_attributes.size(); ++i) {
    std::string value = bool_attributes[i].second ? "true" : "false";
    switch (bool_attributes[i].first) {
      case AX_ATTR_STATE_MIXED:
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
    const std::vector<int32_t>& values = intlist_attributes[i].second;
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
      case AX_ATTR_LINE_BREAKS:
        result += " line_breaks=" + IntVectorToString(values);
        break;
      case AX_ATTR_MARKER_TYPES: {
        std::string types_str;
        for (size_t i = 0; i < values.size(); ++i) {
          auto type = static_cast<AXMarkerType>(values[i]);
          if (type == AX_MARKER_TYPE_NONE)
            continue;

          if (i > 0)
            types_str += ',';

          if (type & AX_MARKER_TYPE_SPELLING)
            types_str += "spelling&";
          if (type & AX_MARKER_TYPE_GRAMMAR)
            types_str += "grammar&";
          if (type & AX_MARKER_TYPE_TEXT_MATCH)
            types_str += "text_match&";

          if (!types_str.empty())
            types_str = types_str.substr(0, types_str.size() - 1);
        }

        if (!types_str.empty())
          result += " marker_types=" + types_str;
        break;
      }
      case AX_ATTR_MARKER_STARTS:
        result += " marker_starts=" + IntVectorToString(values);
        break;
      case AX_ATTR_MARKER_ENDS:
        result += " marker_ends=" + IntVectorToString(values);
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
