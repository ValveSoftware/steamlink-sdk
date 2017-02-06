// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "ui/gfx/transform.h"

namespace content {

AccessibilityTreeFormatterBlink::AccessibilityTreeFormatterBlink()
    : AccessibilityTreeFormatter() {
}

AccessibilityTreeFormatterBlink::~AccessibilityTreeFormatterBlink() {
}

uint32_t AccessibilityTreeFormatterBlink::ChildCount(
    const BrowserAccessibility& node) const {
  if (node.HasIntAttribute(ui::AX_ATTR_CHILD_TREE_ID))
    return node.PlatformChildCount();
  else
    return node.InternalChildCount();
}

BrowserAccessibility* AccessibilityTreeFormatterBlink::GetChild(
    const BrowserAccessibility& node,
    uint32_t i) const {
  if (node.HasIntAttribute(ui::AX_ATTR_CHILD_TREE_ID))
    return node.PlatformGetChild(i);
  else
    return node.InternalGetChild(i);
}

void AccessibilityTreeFormatterBlink::AddProperties(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) {
  dict->SetInteger("id", node.GetId());

  dict->SetString("internalRole", ui::ToString(node.GetData().role));

  gfx::Rect bounds = node.GetData().location;
  dict->SetInteger("boundsX", bounds.x());
  dict->SetInteger("boundsY", bounds.y());
  dict->SetInteger("boundsWidth", bounds.width());
  dict->SetInteger("boundsHeight", bounds.height());

  gfx::Rect page_bounds = node.GetLocalBoundsRect();
  dict->SetInteger("pageBoundsX", page_bounds.x());
  dict->SetInteger("pageBoundsY", page_bounds.y());
  dict->SetInteger("pageBoundsWidth", page_bounds.width());
  dict->SetInteger("pageBoundsHeight", page_bounds.height());

  dict->SetBoolean("transform",
                   node.GetData().transform &&
                   !node.GetData().transform->IsIdentity());

  for (int state_index = ui::AX_STATE_NONE;
       state_index <= ui::AX_STATE_LAST;
       ++state_index) {
    auto state = static_cast<ui::AXState>(state_index);
    if (node.HasState(state))
      dict->SetBoolean(ui::ToString(state), true);
  }

  for (int attr_index = ui::AX_STRING_ATTRIBUTE_NONE;
       attr_index <= ui::AX_STRING_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXStringAttribute>(attr_index);
    if (node.HasStringAttribute(attr))
      dict->SetString(ui::ToString(attr), node.GetStringAttribute(attr));
  }

  for (int attr_index = ui::AX_INT_ATTRIBUTE_NONE;
       attr_index <= ui::AX_INT_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXIntAttribute>(attr_index);
    if (node.HasIntAttribute(attr))
      dict->SetInteger(ui::ToString(attr), node.GetIntAttribute(attr));
  }

  for (int attr_index = ui::AX_FLOAT_ATTRIBUTE_NONE;
       attr_index <= ui::AX_FLOAT_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXFloatAttribute>(attr_index);
    if (node.HasFloatAttribute(attr))
      dict->SetDouble(ui::ToString(attr), node.GetFloatAttribute(attr));
  }

  for (int attr_index = ui::AX_BOOL_ATTRIBUTE_NONE;
       attr_index <= ui::AX_BOOL_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXBoolAttribute>(attr_index);
    if (node.HasBoolAttribute(attr))
      dict->SetBoolean(ui::ToString(attr), node.GetBoolAttribute(attr));
  }

  for (int attr_index = ui::AX_INT_LIST_ATTRIBUTE_NONE;
       attr_index <= ui::AX_INT_LIST_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXIntListAttribute>(attr_index);
    if (node.HasIntListAttribute(attr)) {
      std::vector<int32_t> values;
      node.GetIntListAttribute(attr, &values);
      base::ListValue* value_list = new base::ListValue;
      for (size_t i = 0; i < values.size(); ++i)
        value_list->AppendInteger(values[i]);
      dict->Set(ui::ToString(attr), value_list);
    }
  }
}

base::string16 AccessibilityTreeFormatterBlink::ToString(
    const base::DictionaryValue& dict) {
  base::string16 line;

  if (show_ids()) {
    int id_value;
    dict.GetInteger("id", &id_value);
    WriteAttribute(true, base::IntToString16(id_value), &line);
  }

  base::string16 role_value;
  dict.GetString("internalRole", &role_value);
  WriteAttribute(true, base::UTF16ToUTF8(role_value), &line);

  for (int state_index = ui::AX_STATE_NONE;
       state_index <= ui::AX_STATE_LAST;
       ++state_index) {
    auto state = static_cast<ui::AXState>(state_index);
    const base::Value* value;
    if (!dict.Get(ui::ToString(state), &value))
      continue;

    WriteAttribute(false, ui::ToString(state), &line);
  }

  WriteAttribute(false,
                 FormatCoordinates("location", "boundsX", "boundsY", dict),
                 &line);
  WriteAttribute(false,
                 FormatCoordinates("size", "boundsWidth", "boundsHeight", dict),
                 &line);

  WriteAttribute(false,
                 FormatCoordinates("pageLocation",
                                   "pageBoundsX", "pageBoundsY", dict),
                 &line);
  WriteAttribute(false,
                 FormatCoordinates("pageSize",
                                   "pageBoundsWidth", "pageBoundsHeight", dict),
                 &line);

  bool transform;
  if (dict.GetBoolean("transform", &transform) && transform)
    WriteAttribute(false, "transform", &line);

  for (int attr_index = ui::AX_STRING_ATTRIBUTE_NONE;
       attr_index <= ui::AX_STRING_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXStringAttribute>(attr_index);
    std::string string_value;
    if (!dict.GetString(ui::ToString(attr), &string_value))
      continue;
    WriteAttribute(false,
                   base::StringPrintf(
                       "%s='%s'",
                       ui::ToString(attr).c_str(),
                       string_value.c_str()),
                   &line);
  }

  for (int attr_index = ui::AX_INT_ATTRIBUTE_NONE;
       attr_index <= ui::AX_INT_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXIntAttribute>(attr_index);
    int int_value;
    if (!dict.GetInteger(ui::ToString(attr), &int_value))
      continue;
    WriteAttribute(false,
                   base::StringPrintf(
                       "%s=%d",
                       ui::ToString(attr).c_str(),
                       int_value),
                   &line);
  }

  for (int attr_index = ui::AX_BOOL_ATTRIBUTE_NONE;
       attr_index <= ui::AX_BOOL_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXBoolAttribute>(attr_index);
    bool bool_value;
    if (!dict.GetBoolean(ui::ToString(attr), &bool_value))
      continue;
    WriteAttribute(false,
                   base::StringPrintf(
                       "%s=%s",
                       ui::ToString(attr).c_str(),
                       bool_value ? "true" : "false"),
                   &line);
  }

  for (int attr_index = ui::AX_INT_LIST_ATTRIBUTE_NONE;
       attr_index <= ui::AX_INT_LIST_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXIntListAttribute>(attr_index);
    const base::ListValue* value;
    if (!dict.GetList(ui::ToString(attr), &value))
      continue;
    std::string attr_string = ui::ToString(attr) + "=";
    for (size_t i = 0; i < value->GetSize(); ++i) {
      int int_value;
      value->GetInteger(i, &int_value);
      if (i > 0)
        attr_string += ",";
      attr_string += base::IntToString(int_value);
    }
    WriteAttribute(false, attr_string, &line);
  }

  return line;
}

const base::FilePath::StringType
AccessibilityTreeFormatterBlink::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-blink.txt");
}

const std::string AccessibilityTreeFormatterBlink::GetAllowEmptyString() {
  return "@BLINK-ALLOW-EMPTY:";
}

const std::string AccessibilityTreeFormatterBlink::GetAllowString() {
  return "@BLINK-ALLOW:";
}

const std::string AccessibilityTreeFormatterBlink::GetDenyString() {
  return "@BLINK-DENY:";
}

}  // namespace content
