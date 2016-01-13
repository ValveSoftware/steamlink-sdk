// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter.h"

#include <oleacc.h>

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/base/win/atl_module.h"

using base::StringPrintf;

namespace content {

const char* ALL_ATTRIBUTES[] = {
    "name",
    "value",
    "states",
    "attributes",
    "role_name",
    "currentValue",
    "minimumValue",
    "maximumValue",
    "description",
    "default_action",
    "keyboard_shortcut",
    "location",
    "size",
    "index_in_parent",
    "n_relations",
    "group_level",
    "similar_items_in_group",
    "position_in_group",
    "table_rows",
    "table_columns",
    "row_index",
    "column_index",
    "n_characters",
    "caret_offset",
    "n_selections",
    "selection_start",
    "selection_end"
};

void AccessibilityTreeFormatter::Initialize() {
  ui::win::CreateATLModuleIfNeeded();
}

void AccessibilityTreeFormatter::AddProperties(
    const BrowserAccessibility& node, base::DictionaryValue* dict) {
  BrowserAccessibilityWin* acc_obj =
      const_cast<BrowserAccessibility*>(&node)->ToBrowserAccessibilityWin();

  VARIANT variant_self;
  variant_self.vt = VT_I4;
  variant_self.lVal = CHILDID_SELF;

  dict->SetString("role", IAccessible2RoleToString(acc_obj->ia2_role()));

  CComBSTR msaa_variant;
  HRESULT hresult = acc_obj->get_accName(variant_self, &msaa_variant);
  if (hresult == S_OK)
    dict->SetString("name", msaa_variant.m_str);
  hresult = acc_obj->get_accValue(variant_self, &msaa_variant);
  if (hresult == S_OK)
    dict->SetString("value", msaa_variant.m_str);

  std::vector<base::string16> state_strings;
  int32 ia_state = acc_obj->ia_state();

  // Avoid flakiness: these states depend on whether the window is focused
  // and the position of the mouse cursor.
  ia_state &= ~STATE_SYSTEM_HOTTRACKED;
  ia_state &= ~STATE_SYSTEM_OFFSCREEN;

  IAccessibleStateToStringVector(ia_state, &state_strings);
  IAccessible2StateToStringVector(acc_obj->ia2_state(), &state_strings);
  base::ListValue* states = new base::ListValue;
  for (std::vector<base::string16>::const_iterator it = state_strings.begin();
       it != state_strings.end();
       ++it) {
    states->AppendString(base::UTF16ToUTF8(*it));
  }
  dict->Set("states", states);

  const std::vector<base::string16>& ia2_attributes = acc_obj->ia2_attributes();
  base::ListValue* attributes = new base::ListValue;
  for (std::vector<base::string16>::const_iterator it = ia2_attributes.begin();
       it != ia2_attributes.end();
       ++it) {
    attributes->AppendString(base::UTF16ToUTF8(*it));
  }
  dict->Set("attributes", attributes);

  dict->SetString("role_name", acc_obj->role_name());

  VARIANT currentValue;
  if (acc_obj->get_currentValue(&currentValue) == S_OK)
    dict->SetDouble("currentValue", V_R8(&currentValue));

  VARIANT minimumValue;
  if (acc_obj->get_minimumValue(&minimumValue) == S_OK)
    dict->SetDouble("minimumValue", V_R8(&minimumValue));

  VARIANT maximumValue;
  if (acc_obj->get_maximumValue(&maximumValue) == S_OK)
    dict->SetDouble("maximumValue", V_R8(&maximumValue));

  hresult = acc_obj->get_accDescription(variant_self, &msaa_variant);
  if (hresult == S_OK)
    dict->SetString("description", msaa_variant.m_str);

  hresult = acc_obj->get_accDefaultAction(variant_self, &msaa_variant);
  if (hresult == S_OK)
    dict->SetString("default_action", msaa_variant.m_str);

  hresult = acc_obj->get_accKeyboardShortcut(variant_self, &msaa_variant);
  if (hresult == S_OK)
    dict->SetString("keyboard_shortcut", msaa_variant.m_str);

  hresult = acc_obj->get_accHelp(variant_self, &msaa_variant);
  if (S_OK == hresult)
    dict->SetString("help", msaa_variant.m_str);

  BrowserAccessibility* root = node.manager()->GetRoot();
  LONG left, top, width, height;
  LONG root_left, root_top, root_width, root_height;
  if (acc_obj->accLocation(&left, &top, &width, &height, variant_self)
      != S_FALSE
      && root->ToBrowserAccessibilityWin()->accLocation(
          &root_left, &root_top, &root_width, &root_height, variant_self)
      != S_FALSE) {
    base::DictionaryValue* location = new base::DictionaryValue;
    location->SetInteger("x", left - root_left);
    location->SetInteger("y", top - root_top);
    dict->Set("location", location);

    base::DictionaryValue* size = new base::DictionaryValue;
    size->SetInteger("width", width);
    size->SetInteger("height", height);
    dict->Set("size", size);
  }

  LONG index_in_parent;
  if (acc_obj->get_indexInParent(&index_in_parent) == S_OK)
    dict->SetInteger("index_in_parent", index_in_parent);

  LONG n_relations;
  if (acc_obj->get_nRelations(&n_relations) == S_OK)
    dict->SetInteger("n_relations", n_relations);

  LONG group_level, similar_items_in_group, position_in_group;
  if (acc_obj->get_groupPosition(&group_level,
                                 &similar_items_in_group,
                                 &position_in_group) == S_OK) {
    dict->SetInteger("group_level", group_level);
    dict->SetInteger("similar_items_in_group", similar_items_in_group);
    dict->SetInteger("position_in_group", position_in_group);
  }
  LONG table_rows;
  if (acc_obj->get_nRows(&table_rows) == S_OK)
    dict->SetInteger("table_rows", table_rows);
  LONG table_columns;
  if (acc_obj->get_nRows(&table_columns) == S_OK)
    dict->SetInteger("table_columns", table_columns);
  LONG row_index;
  if (acc_obj->get_rowIndex(&row_index) == S_OK)
    dict->SetInteger("row_index", row_index);
  LONG column_index;
  if (acc_obj->get_columnIndex(&column_index) == S_OK)
    dict->SetInteger("column_index", column_index);
  LONG n_characters;
  if (acc_obj->get_nCharacters(&n_characters) == S_OK)
    dict->SetInteger("n_characters", n_characters);
  LONG caret_offset;
  if (acc_obj->get_caretOffset(&caret_offset) == S_OK)
    dict->SetInteger("caret_offset", caret_offset);
  LONG n_selections;
  if (acc_obj->get_nSelections(&n_selections) == S_OK) {
    dict->SetInteger("n_selections", n_selections);
    if (n_selections > 0) {
      LONG start, end;
      if (acc_obj->get_selection(0, &start, &end) == S_OK) {
        dict->SetInteger("selection_start", start);
        dict->SetInteger("selection_end", end);
      }
    }
  }
}

base::string16 AccessibilityTreeFormatter::ToString(
    const base::DictionaryValue& dict,
    const base::string16& indent) {
  base::string16 line;

  base::string16 role_value;
  dict.GetString("role", &role_value);
  WriteAttribute(true, base::UTF16ToUTF8(role_value), &line);

  base::string16 name_value;
  dict.GetString("name", &name_value);
  WriteAttribute(true, base::StringPrintf(L"name='%ls'", name_value.c_str()),
                 &line);

  for (int i = 0; i < arraysize(ALL_ATTRIBUTES); i++) {
    const char* attribute_name = ALL_ATTRIBUTES[i];
    const base::Value* value;
    if (!dict.Get(attribute_name, &value))
      continue;

    switch (value->GetType()) {
      case base::Value::TYPE_STRING: {
        base::string16 string_value;
        value->GetAsString(&string_value);
        WriteAttribute(false,
                       StringPrintf(L"%ls='%ls'",
                                    base::UTF8ToUTF16(attribute_name).c_str(),
                                    string_value.c_str()),
                       &line);
        break;
      }
      case base::Value::TYPE_INTEGER: {
        int int_value;
        value->GetAsInteger(&int_value);
        WriteAttribute(false,
                       base::StringPrintf(L"%ls=%d",
                                          base::UTF8ToUTF16(
                                              attribute_name).c_str(),
                                          int_value),
                       &line);
        break;
      }
      case base::Value::TYPE_DOUBLE: {
        double double_value;
        value->GetAsDouble(&double_value);
        WriteAttribute(false,
                       base::StringPrintf(L"%ls=%.2f",
                                          base::UTF8ToUTF16(
                                              attribute_name).c_str(),
                                          double_value),
                       &line);
        break;
      }
      case base::Value::TYPE_LIST: {
        // Currently all list values are string and are written without
        // attribute names.
        const base::ListValue* list_value;
        value->GetAsList(&list_value);
        for (base::ListValue::const_iterator it = list_value->begin();
             it != list_value->end();
             ++it) {
          base::string16 string_value;
          if ((*it)->GetAsString(&string_value))
            WriteAttribute(false, string_value, &line);
        }
        break;
      }
      case base::Value::TYPE_DICTIONARY: {
        // Currently all dictionary values are coordinates.
        // Revisit this if that changes.
        const base::DictionaryValue* dict_value;
        value->GetAsDictionary(&dict_value);
        if (strcmp(attribute_name, "size") == 0) {
          WriteAttribute(false,
                         FormatCoordinates("size", "width", "height",
                                           *dict_value),
                         &line);
        } else if (strcmp(attribute_name, "location") == 0) {
          WriteAttribute(false,
                         FormatCoordinates("location", "x", "y", *dict_value),
                         &line);
        }
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }

  return indent + line + base::ASCIIToUTF16("\n");
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetActualFileSuffix() {
  return FILE_PATH_LITERAL("-actual-win.txt");
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-win.txt");
}

// static
const std::string AccessibilityTreeFormatter::GetAllowEmptyString() {
  return "@WIN-ALLOW-EMPTY:";
}

// static
const std::string AccessibilityTreeFormatter::GetAllowString() {
  return "@WIN-ALLOW:";
}

// static
const std::string AccessibilityTreeFormatter::GetDenyString() {
  return "@WIN-DENY:";
}

}  // namespace content
